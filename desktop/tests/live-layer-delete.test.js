import assert from 'node:assert/strict';
import test from 'node:test';
import { deserialize, exportSVG, serialize } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createLiveLayerDeletion } from '../src/live-layer-delete.js';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';
import { orderingDocument } from './fixtures/artwork-ordering.js';

function setup(doc, drop = false) {
  const owner = createCompositionSessionOwner(
    deserializeCompositionProject(serialize(doc), deserialize),
  );
  const readTarget = () => ({
    documentGeneration: 1,
    compositionId: owner.activeCompositionId,
    sessionId: owner.state.sessionId,
    sessionRevision: owner.state.expectedRevision,
    projectRevision: owner.projectRevision,
  });
  let busy = false;
  let calls = 0;
  const handle = createLiveEditorProtocol({
    readTarget,
    deleteLayer: createLiveLayerDeletion({
      readTarget,
      getOwner: () => owner,
      isEditBlocked: () => busy,
      commit(active, envelope, layerId) {
        assert.equal(active, owner);
        calls++;
        const receipt = owner.dispatch({ ...envelope, type: 'delete-layer', payload: { layerId } });
        return drop ? null : receipt;
      },
    }),
  });
  const remove = (layerId, confirmed = true, target = readTarget()) =>
    handle({ version: 1, operation: 'layer.delete', arguments: { target, layerId, confirmed } });
  return { owner, readTarget, remove, calls: () => calls, block: () => (busy = !busy) };
}
const snapshot = (owner) => serializeCompositionProject(owner.projectSnapshot());

test('exact layer deletion requires confirmation, respects lock and shares GUI history', () => {
  const doc = orderingDocument();
  const [red, green, blue] = doc.layers;
  blue.locked = true;
  const f = setup(doc),
    gui = setup(doc);
  const initial = snapshot(f.owner),
    target = f.readTarget();
  assert.equal(f.remove(green.id, false).status, 'invalid-arguments');
  assert.equal(f.remove('missing').status, 'unsupported-target');
  assert.equal(f.remove(blue.id).status, 'rejected');
  f.block();
  assert.equal(f.remove(green.id).status, 'edit-in-progress');
  f.block();
  assert.equal(f.calls(), 0);
  assert.equal(snapshot(f.owner), initial);
  const outcome = f.remove(green.id);
  assert.equal(outcome.status, 'changed');
  assert.equal(outcome.removedLayerId, green.id);
  assert.equal(outcome.nextLayerId, blue.id);
  assert.equal(outcome.replacedLastLayer, false);
  assert.equal(outcome.target.projectRevision, target.projectRevision + 1);
  gui.owner.dispatch({ ...gui.owner.state, type: 'delete-layer', payload: { layerId: green.id } });
  assert.equal(snapshot(f.owner), snapshot(gui.owner));
  assert.equal(exportSVG(f.owner.snapshot()), exportSVG(gui.owner.snapshot()));
  assert.deepEqual(
    f.owner.snapshot().layers.map(({ id }) => id),
    [red.id, blue.id],
  );
  const after = snapshot(f.owner);
  assert.equal(f.remove(green.id, true, target).status, 'stale-target');
  f.owner.dispatch({ ...f.owner.state, type: 'undo' });
  assert.equal(snapshot(f.owner), initial);
  f.owner.dispatch({ ...f.owner.state, type: 'redo' });
  assert.equal(snapshot(f.owner), after);
});

test('last-layer deletion preserves one blank Output and cannot replay a dropped receipt', () => {
  const doc = orderingDocument();
  doc.layers = doc.layers.slice(0, 1);
  const [prior] = doc.layers;
  const f = setup(doc, true),
    target = f.readTarget();
  assert.equal(f.remove(prior.id).status, 'outcome-unknown');
  const [blank] = f.owner.snapshot().layers;
  assert.notEqual(blank.id, prior.id);
  assert.deepEqual(
    blank.nodes.map((n) => n.type),
    ['Output'],
  );
  assert.equal(f.remove(prior.id, true, target).status, 'stale-target');
  assert.equal(f.calls(), 1);
  f.owner.dispatch({ ...f.owner.state, type: 'undo' });
  assert.equal(f.owner.snapshot().layers[0].id, prior.id);
});
