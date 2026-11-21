import assert from 'node:assert/strict';
import test from 'node:test';
import { createLiveArtworkOutput } from '../src/live-artwork-output.js';

function fixture() {
  const target = {
    documentGeneration: 0,
    compositionId: 'A',
    sessionId: 'one',
    sessionRevision: 0,
    projectRevision: 0,
  };
  let onExport = async () => ({ written: true });
  const calls = [];
  const operation = createLiveArtworkOutput({
    readTarget: () => target,
    readSnapshot: () => ({ document: { composition: target.compositionId }, fonts: {} }),
    exportArtwork(request, write) {
      calls.push(request);
      return onExport(request, write);
    },
    write: () => true,
  });
  return { operation, target, calls, setExport: (fn) => (onExport = fn) };
}

test('switch away and back invalidates the old request without entering the renderer', async () => {
  const { operation, target, calls } = fixture();
  const original = operation.inspect();
  target.compositionId = 'B';
  target.sessionId = 'two';
  assert.deepEqual(await operation.run({ target: original, format: 'svg' }), {
    status: 'stale-target',
  });
  target.compositionId = 'A';
  target.sessionId = 'three';
  assert.deepEqual(await operation.run({ target: original, format: 'svg' }), {
    status: 'stale-target',
  });
  assert.equal(calls.length, 0);
  const result = await operation.run({ target: operation.inspect(), format: 'svg' });
  assert.equal(result.status, 'accepted-by-writer');
  assert.equal(calls[0].document.composition, 'A');
});

test('second request is busy until cancellation, then a new request can run', async () => {
  const { operation, calls, setExport, target } = fixture();
  const { promise, resolve } = Promise.withResolvers();
  setExport(() => promise);
  const first = operation.run({ target: operation.inspect(), format: 'png' });
  target.compositionId = 'B';
  target.sessionId = 'two';
  assert.deepEqual(await operation.run({ target: operation.inspect(), format: 'png' }), {
    status: 'busy',
  });
  assert.equal(calls.length, 1);
  assert.equal(calls[0].document.composition, 'A');
  resolve({ written: false });
  assert.equal((await first).status, 'canceled');
  setExport(async () => ({ written: true }));
  assert.equal(
    (await operation.run({ target: operation.inspect(), format: 'png' })).status,
    'accepted-by-writer',
  );
  assert.equal(calls[1].document.composition, 'B');
});

test('failed write releases the same shared admission for a retry', async () => {
  const { operation, setExport } = fixture();
  setExport(async () => {
    throw Error('write failed');
  });
  const intent = { target: operation.inspect(), format: 'svg' };
  await assert.rejects(operation.run(intent), /write failed/);
  setExport(async () => ({ written: true }));
  assert.equal((await operation.run(intent)).status, 'accepted-by-writer');
});
