import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import { DocumentLifecycle } from '../src/document-lifecycle.js';
import { createLiveDocumentSave } from '../src/live-document-save.js';

function harness(write) {
  let current = { document: 'first' };
  let draft = false;
  const documents = new DocumentLifecycle({
    read: () => current,
    write,
    hasDraft: () => draft,
  });
  const readTarget = () => ({
    documentGeneration: documents.generation,
    sessionRevision: 0,
    compositionId: 'composition',
    sessionId: 'session',
    projectRevision: 0,
  });
  return {
    documents,
    readTarget,
    save: createLiveDocumentSave({ readTarget, documents }),
    draft: (value) => {
      draft = value;
    },
    change: (value) => {
      current = value;
    },
  };
}

describe('live document save', () => {
  it('does not write for stale targets, active drafts, or an invalid save mode', async () => {
    const h = harness(() => {
      throw Error('unexpected write');
    });
    const target = h.readTarget();
    await h.documents.replace();
    assert.deepEqual(await h.save({ target, saveAs: true }), { status: 'stale-target' });
    assert.deepEqual(await h.save({ target: h.readTarget(), saveAs: 'true' }), {
      status: 'invalid-arguments',
    });
    h.draft(true);
    assert.deepEqual(await h.save({ target: h.readTarget(), saveAs: true }), {
      status: 'edit-in-progress',
    });
  });

  it('saves the admitted snapshot once and keeps edits during the picker dirty', async () => {
    let release;
    const writes = [];
    const previous = { token: 'previous' };
    const binding = { token: 'new' };
    const h = harness((packet) => {
      writes.push(packet);
      return new Promise((resolve) => {
        release = resolve;
      });
    });
    await h.documents.replace({ binding: previous, saved: true });
    const target = h.readTarget();
    const first = h.save({ target, saveAs: true });
    assert.deepEqual(await h.save({ target, saveAs: true }), { status: 'busy' });
    assert.deepEqual(writes, [{ contents: { document: 'first' }, binding: null, operation: 1 }]);
    h.change({ document: 'second' });
    release(binding);
    assert.deepEqual(await first, { status: 'accepted-by-writer' });
    assert.equal(h.documents.binding, binding);
    assert.equal(h.documents.dirty, true);
  });

  it('preserves dirty state and accepts a later attempt after cancellation or writer failure', async () => {
    let calls = 0;
    const binding = { token: 'original' };
    const h = harness(({ binding: existing }) => {
      assert.equal(existing, binding);
      if (++calls === 1) return null;
      if (calls === 2) throw Error('write failed');
      return binding;
    });
    await h.documents.replace({ binding });
    const intent = { target: h.readTarget(), saveAs: false };
    assert.deepEqual(await h.save(intent), { status: 'not-confirmed' });
    await assert.rejects(h.save(intent), /write failed/);
    assert.equal(h.documents.dirty, true);
    assert.deepEqual(await h.save(intent), { status: 'accepted-by-writer' });
    assert.equal(h.documents.dirty, false);
  });
});
