import assert from 'node:assert/strict';
import test from 'node:test';
import { readFileSync } from 'node:fs';
import { createArtworkExporter } from '../src/artwork-export.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { deserialize, exportSVG } from '../src/core.js';
import { DocumentLifecycle } from '../src/document-lifecycle.js';
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

test('the live Composition owner rejects stale output without changing authored state', async () => {
  const raw = readFileSync(
    new URL('../examples/shared-color-reference.json', import.meta.url),
    'utf8',
  );
  const project = deserializeCompositionProject(raw, deserialize);
  const owner = createCompositionSessionOwner(project);
  const original = owner.activeCompositionId;
  const alternate = project.compositions.find((c) => c.id !== original).id;
  const outputs = [];
  const read = () => serializeCompositionProject(owner.projectSnapshot());
  const lifecycle = new DocumentLifecycle({
    read,
    choose: async () => assert.fail('must not open a document prompt'),
    write: async () => assert.fail('must not save the project'),
  });
  await lifecycle.replace({ saved: true });
  const output = createLiveArtworkOutput({
    readTarget: () => ({
      documentGeneration: lifecycle.generation,
      compositionId: owner.activeCompositionId,
      sessionId: owner.state.sessionId,
      sessionRevision: owner.state.expectedRevision,
      projectRevision: owner.projectRevision,
    }),
    readSnapshot: () => ({ document: owner.snapshot(), fonts: {} }),
    exportArtwork: createArtworkExporter({ exportSVG }),
    write: async (svg) => (outputs.push(svg), true),
  });
  const snapshot = () => [read(), owner.historyState, lifecycle.dirty, outputs.length];
  const verifyStale = async (target) => {
    const before = snapshot();
    assert.deepEqual(await output.run({ format: 'svg', target }), { status: 'stale-target' });
    assert.deepEqual(snapshot(), before);
  };
  const first = output.inspect();
  owner.switchComposition(alternate);
  await verifyStale(first);
  owner.switchComposition(original);
  await verifyStale(first);
  assert.equal(first.projectRevision, owner.projectRevision);

  const beforeEdit = output.inspect();
  owner.dispatch({
    ...owner.state,
    type: 'set-document-settings',
    payload: { key: 'width', value: 1011 },
  });
  assert.equal(lifecycle.dirty, true);
  await verifyStale(beforeEdit);
  const beforeFileBindingChange = output.inspect();
  await lifecycle.replace({ saved: true });
  assert.equal(lifecycle.dirty, false);
  await verifyStale(beforeFileBindingChange);
  assert.equal(outputs.length, 0);

  const before = snapshot().slice(0, 3);
  assert.equal(
    (await output.run({ format: 'svg', target: output.inspect() })).status,
    'accepted-by-writer',
  );
  assert.deepEqual(snapshot().slice(0, 3), before);
  assert.deepEqual(outputs, [exportSVG(owner.snapshot(), {})]);
});
