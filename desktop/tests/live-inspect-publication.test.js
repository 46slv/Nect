import assert from 'node:assert/strict';
import test from 'node:test';
import { DocumentLifecycle } from '../src/document-lifecycle.js';
import { createLiveInspectPublication } from '../src/live-inspect-publication.js';

const first = Object.freeze({
  documentGeneration: 1,
  compositionId: 'a',
  sessionId: 'editor-a',
  sessionRevision: 2,
  projectRevision: 3,
});

test('only the current in-process owner can take its capture, exactly once', () => {
  const a = {};
  const b = {};
  let reads = 0;
  const gate = createLiveInspectPublication({
    readTarget: () => {
      reads++;
      return first;
    },
    canInspect: () => true,
  });
  assert.equal(gate.prepare(a), null);
  assert.equal(reads, 0);
  gate.bind(a);
  const old = gate.prepare(a);
  assert.ok(old);
  assert.equal(gate.prepare(a), null);
  assert.equal(gate.take(b, old), null);
  gate.bind(b);
  const current = gate.prepare(b);
  assert.equal(gate.take(a, old), null);
  assert.equal(gate.take(b, old), null);
  gate.revoke(a);
  assert.deepEqual(gate.take(b, current), { status: 'ok', target: first });
  assert.equal(gate.take(b, current), null);
  const cancelled = gate.prepare(b);
  gate.revoke(b);
  gate.bind(b);
  assert.equal(gate.take(b, cancelled), null);
});

test('a tab transition, return to the tab, and revision changes all invalidate a capture', () => {
  const owner = {};
  let target = first;
  let ready = true;
  const gate = createLiveInspectPublication({
    readTarget: () => target,
    canInspect: () => ready,
  });
  gate.bind(owner);
  const whileMoving = gate.prepare(owner);
  ready = false;
  assert.equal(gate.take(owner, whileMoving), null);
  assert.equal(gate.prepare(owner), null);
  ready = true;

  const beforeSwitch = gate.prepare(owner);
  target = { ...first, documentGeneration: 3 };
  assert.equal(gate.take(owner, beforeSwitch), null);
  const beforeEdit = gate.prepare(owner);
  target = { ...target, sessionRevision: 4 };
  assert.equal(gate.take(owner, beforeEdit), null);
  const fresh = gate.prepare(owner);
  assert.deepEqual(gate.take(owner, fresh), { status: 'ok', target });
  assert.equal(first.sessionRevision, 2);
});

test('uses the real document lifecycle without editing content or opening the writer', async () => {
  let text = 'first document with unsaved edits';
  let decision;
  const documents = new DocumentLifecycle({
    read: () => text,
    write: () => assert.fail('Inspect must not write a document'),
    choose: () => new Promise((resolve) => (decision = resolve)),
  });
  const owner = {};
  const readTarget = () => ({ ...first, documentGeneration: documents.generation });
  const gate = createLiveInspectPublication({
    readTarget,
    canInspect: () => !documents.transitioning,
  });
  gate.bind(owner);

  const beforeTransition = gate.prepare(owner);
  assert.ok(beforeTransition);
  const transition = documents.guard('open');
  assert.equal(documents.transitioning, true);
  assert.equal(gate.take(owner, beforeTransition), null);
  assert.equal(gate.prepare(owner), null);
  decision('cancel');
  assert.equal(await transition, false);
  assert.equal(text, 'first document with unsaved edits');

  const beforeReplacement = gate.prepare(owner);
  text = 'different saved document';
  await documents.replace({ saved: true });
  assert.equal(gate.take(owner, beforeReplacement), null);
  const beforeRead = {
    text,
    revision: documents.revision,
    saved: documents.saved,
    epoch: documents.saveEpoch,
  };
  const current = gate.prepare(owner);
  assert.deepEqual(gate.take(owner, current), { status: 'ok', target: readTarget() });
  assert.deepEqual(
    { text, revision: documents.revision, saved: documents.saved, epoch: documents.saveEpoch },
    beforeRead,
  );
});
