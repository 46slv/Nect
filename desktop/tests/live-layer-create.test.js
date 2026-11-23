import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import { deserialize, exportSVG } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createLiveBlankLayer } from '../src/live-layer-create.js';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';

const seed = readFileSync(new URL('../examples/blend-distance.json', import.meta.url), 'utf8');
function fixture({ loseReceipt = false } = {}) {
  const owner = createCompositionSessionOwner(deserializeCompositionProject(seed, deserialize));
  const readTarget = () => ({
    documentGeneration: 1,
    compositionId: owner.activeCompositionId,
    sessionId: owner.state.sessionId,
    sessionRevision: owner.state.expectedRevision,
    projectRevision: owner.projectRevision,
  });
  let calls = 0;
  let busy = false;
  const host = createLiveBlankLayer({
    readTarget,
    getOwner: () => owner,
    isEditBlocked: () => busy,
    commit(active, command) {
      calls++;
      assert.equal(active, owner);
      assert.equal(command.type, 'create-blank-layer');
      const receipt = active.dispatch(command);
      return loseReceipt ? null : receipt;
    },
  });
  const handle = createLiveEditorProtocol({ readTarget, createBlankLayer: host });
  return { owner, readTarget, handle, calls: () => calls, block: (value) => (busy = value) };
}
const request = (target, name) => ({
  version: 1,
  operation: 'layer.createBlank',
  arguments: { target, name },
});
const snapshot = (owner) => serializeCompositionProject(owner.projectSnapshot());

test('blank layer creation adds one empty Output, preserves artwork exports and history', () => {
  const f = fixture();
  const target = f.readTarget();
  const before = snapshot(f.owner);
  const oldSvg = exportSVG(f.owner.snapshot());
  const entry = request(target, '  Draft  ');
  assert.equal(f.handle(request(target, '  ')).status, 'invalid-arguments');
  assert.equal(f.handle(request(target, 'x'.repeat(121))).status, 'invalid-arguments');
  assert.equal(f.handle(request(target, 42)).status, 'invalid-arguments');
  f.block(true);
  assert.equal(f.handle(entry).status, 'edit-in-progress');
  f.block(false);
  assert.equal(snapshot(f.owner), before);
  assert.equal(f.calls(), 0);
  const created = f.handle(entry);
  assert.equal(created.status, 'changed');
  const doc = f.owner.snapshot();
  assert.equal(created.target.projectRevision, target.projectRevision + 1);
  const layer = doc.layers.at(-1);
  assert.equal(layer.id, created.layerId);
  assert.equal(layer.name, 'Draft');
  assert.equal(layer.output, created.nodeId);
  assert.deepEqual(
    layer.nodes.map(({ type }) => type),
    ['Output'],
  );
  assert.equal(exportSVG(doc), oldSvg);
  const authored = snapshot(f.owner);
  assert.equal(f.handle(entry).status, 'stale-target');
  f.owner.dispatch({ ...f.owner.state, type: 'undo' });
  assert.equal(snapshot(f.owner), before);
  f.owner.dispatch({ ...f.owner.state, type: 'redo' });
  assert.equal(snapshot(f.owner), authored);
});

test('blank layer receipt loss changes the document once and stale replay cannot create another', () => {
  const f = fixture({ loseReceipt: true });
  const entry = request(f.readTarget(), null);
  assert.equal(f.handle(entry).status, 'outcome-unknown');
  assert.equal(f.owner.snapshot().layers.length, 2);
  assert.equal(f.owner.snapshot().layers.at(-1).name, 'Composition 2');
  assert.equal(f.handle(entry).status, 'stale-target');
  assert.equal(f.calls(), 1);
});
