import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import { DocumentLifecycle } from '../src/document-lifecycle.js';
import { createLiveDocumentSave, createLiveSaveAsBoundary } from '../src/live-document-save.js';

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

it('readback distinguishes a completed Save As from an uncommitted retry without exposing a path', async () => {
  let release;
  let writes = 0;
  const h = harness(() => {
    ++writes;
    return new Promise((resolve) => {
      release = resolve;
    });
  });
  const previous = { token: 'previous' };
  const destination = { token: 'private-token', path: 'private-path.json' };
  await h.documents.replace({ binding: previous, saved: true });
  h.change({ document: 'edited' });
  const api = createLiveSaveAsBoundary(h);
  assert.deepEqual(api.inspect(), { saveEpoch: 0, dirty: true, pending: false });
  const request = { target: h.readTarget(), saveEpoch: api.inspect().saveEpoch, requestId: 'a' };
  const pending = api.run(request);
  assert.deepEqual(api.inspect(), { saveEpoch: 0, dirty: true, pending: true });
  assert.deepEqual(api.getReceipt('a'), { status: 'pending' });
  assert.deepEqual(await api.run(request), { status: 'pending' });
  assert.deepEqual(await api.run({ ...request, requestId: 'b' }), { status: 'busy' });
  assert.deepEqual(api.getReceipt('b'), { status: 'unknown-request' });
  release(destination);
  // Simulate a caller that never receives the first returned envelope.
  await pending;
  assert.deepEqual(h.readTarget(), request.target);
  assert.deepEqual(api.inspect(), { saveEpoch: 1, dirty: false, pending: false });
  assert.equal(h.documents.binding, destination);
  const committed = { status: 'accepted-by-writer', saveEpoch: 1 };
  assert.deepEqual(api.getReceipt('a'), committed);
  assert.deepEqual(await api.run(request), committed);
  assert.deepEqual(await api.run({ ...request, saveEpoch: 1 }), { status: 'request-conflict' });
  assert.deepEqual(await api.run({ ...request, requestId: 'b' }), { status: 'stale-save-state' });
  assert.equal(writes, 1);
});

it('save epoch does not advance for cancellation or an unconfirmed writer', async () => {
  let writes = 0;
  const h = harness(() => {
    if (++writes === 1) return null;
    throw Error('private-path');
  });
  const api = createLiveSaveAsBoundary({ ...h, receiptLimit: 2 });
  const request = { target: h.readTarget(), saveEpoch: 0, requestId: 'a' };
  assert.deepEqual(await api.run({ ...request, saveEpoch: -1 }), { status: 'invalid-arguments' });
  assert.deepEqual(await api.run(request), { status: 'not-confirmed' });
  const next = { ...request, requestId: 'b' };
  assert.deepEqual(await api.run(next), { status: 'outcome-unknown' });
  assert.deepEqual(api.getReceipt('b'), { status: 'outcome-unknown' });
  assert.deepEqual(await api.run(next), { status: 'outcome-unknown' });
  assert.deepEqual(await api.run({ ...request, requestId: 'c' }), { status: 'receipt-capacity' });
  assert.deepEqual(api.getReceipt('a'), { status: 'not-confirmed' });
  assert.deepEqual(api.inspect(), { saveEpoch: 0, dirty: true, pending: false });
  assert.equal(writes, 2);
});
