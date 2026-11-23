import assert from 'node:assert/strict';
import test from 'node:test';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { deserialize, serialize } from '../src/core.js';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';
import { createLiveFolderPlanSession } from '../src/live-folder-plan-session.js';
import { orderingDocument } from './fixtures/artwork-ordering.js';

test('reviewed folder paint plan is single use, exact, and commits through the same Session', () => {
  const seed = orderingDocument();
  const [back, middle, front] = seed.layers;
  seed.editor.layerFolders = [
    { id: 'group', name: 'Group', members: seed.layers.map(({ id }) => ({ layerId: id })) },
  ];
  const owner = createCompositionSessionOwner(
    deserializeCompositionProject(serialize(seed), deserialize),
  );
  let generation = 1;
  let writes = 0;
  const readTarget = () => ({
    documentGeneration: generation,
    compositionId: owner.activeCompositionId,
    sessionId: owner.state.sessionId,
    sessionRevision: owner.state.expectedRevision,
    projectRevision: owner.projectRevision,
  });
  const lane = createLiveFolderPlanSession({
    readTarget,
    readDocument: () => owner.snapshot(),
    getOwner: () => owner,
    isEditBlocked: () => false,
    commit(active, command) {
      assert.equal(active, owner);
      assert.equal(command.type, 'remove-layers-from-folder');
      assert.deepEqual(command.payload.expectedPaintOrder, [back.id, front.id, middle.id]);
      writes++;
      return active.dispatch(command);
    },
  });
  const handle = createLiveEditorProtocol({
    readTarget,
    readLayerFolderPlan: lane.preview,
    applyLayerFolderPlan: lane.apply,
  });
  const invoke = (name, args) =>
    handle({ version: 1, operation: 'layerFolders.' + name, arguments: args });
  const prepare = () =>
    invoke('plan', {
      target: readTarget(),
      request: { mode: 'remove', folderId: 'group', layerIds: [middle.id] },
    });
  const commit = ({ target, planId }, extra = {}) =>
    invoke('applyPlan', { target, planId, confirmed: true, ...extra });
  const snapshot = () => serializeCompositionProject(owner.projectSnapshot());
  const before = snapshot();
  assert.equal(prepare().status, 'ok');
  const obsolete = prepare();
  const later = prepare();
  assert.equal(commit(obsolete).status, 'unknown-plan');
  assert.equal(commit(later, { confirmed: false }).status, 'invalid-arguments');
  assert.equal(commit(later, { layerIds: [front.id] }).status, 'invalid-arguments');
  generation++;
  assert.equal(commit(later).status, 'stale-target');
  assert.equal(commit(later).status, 'unknown-plan');
  const discarded = prepare();
  lane.revoke();
  assert.equal(commit(discarded).status, 'unknown-plan');
  const current = prepare();
  assert.deepEqual(current.after, [back.id, front.id, middle.id]);
  assert.deepEqual(current.membership, [{ folderId: 'group', layerIds: [back.id, front.id] }]);
  assert.equal(writes, 0);
  assert.equal(snapshot(), before);
  assert.equal(commit(current).status, 'changed');
  assert.equal(commit(current).status, 'unknown-plan');
  assert.equal(writes, 1);
  assert.deepEqual(
    owner.snapshot().layers.map(({ id }) => id),
    current.after,
  );
  const after = snapshot();
  owner.dispatch({ ...owner.state, type: 'undo' });
  assert.equal(snapshot(), before);
  owner.dispatch({ ...owner.state, type: 'redo' });
  assert.equal(snapshot(), after);
});
