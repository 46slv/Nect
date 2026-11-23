import assert from 'node:assert/strict';
import test from 'node:test';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { deserialize, serialize } from '../src/core.js';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';
import { createLiveLayerFolderMembership } from '../src/live-layer-folder-membership.js';
import { orderingDocument } from './fixtures/artwork-ordering.js';

test('folder membership API changes grouping without silently changing paint order', () => {
  const seed = orderingDocument();
  const [back, middle, front] = seed.layers;
  seed.editor.layerFolders = [{ id: 'group', name: 'Group', members: [{ layerId: back.id }] }];
  const owner = createCompositionSessionOwner(
    deserializeCompositionProject(serialize(seed), deserialize),
  );
  const snapshot = () => serializeCompositionProject(owner.projectSnapshot());
  const readTarget = () => ({
    documentGeneration: 1,
    compositionId: owner.activeCompositionId,
    sessionId: owner.state.sessionId,
    sessionRevision: owner.state.expectedRevision,
    projectRevision: owner.projectRevision,
  });
  let blocked = false;
  const commands = [];
  const handler = createLiveEditorProtocol({
    readTarget,
    setLayerFolderMembership: createLiveLayerFolderMembership({
      readTarget,
      getOwner: () => owner,
      isEditBlocked: () => blocked,
      commit(active, command) {
        assert.equal(active, owner);
        commands.push(command);
        try {
          return active.dispatch(command);
        } catch (error) {
          assert.match(error.message, /Select Layers adjacent to this Folder/);
          return null;
        }
      },
    }),
  });
  const initial = readTarget();
  const request = {
    version: 1,
    operation: 'layerFolders.changeMembership',
    arguments: { target: initial, mode: 'add', folderId: 'group', layerIds: [middle.id, front.id] },
  };
  const edit = (args = {}) => handler({ ...request, arguments: { ...request.arguments, ...args } });
  assert.equal(createLiveEditorProtocol({ readTarget })(request).status, 'unsupported-operation');
  assert.equal(edit({ expectedPaintOrder: [] }).status, 'invalid-arguments');
  assert.equal(edit({ layerIds: [] }).status, 'invalid-arguments');
  assert.equal(edit({ layerIds: [middle.id, middle.id] }).status, 'invalid-arguments');
  assert.equal(edit({ folderId: 'missing' }).status, 'unsupported-target');
  blocked = true;
  assert.equal(edit().status, 'edit-in-progress');
  blocked = false;
  assert.equal(commands.length, 0);
  const initialSnapshot = snapshot();
  assert.equal(edit({ layerIds: [front.id] }).status, 'rejected');
  assert.equal(snapshot(), initialSnapshot);
  const added = edit();
  assert.equal(added.status, 'changed');
  assert.equal(commands.at(-1).type, 'add-layers-to-folder');
  assert.equal(edit().status, 'stale-target');
  const beforeRemoval = snapshot();
  const count = commands.length;
  assert.equal(
    edit({ target: added.target, mode: 'remove', layerIds: [middle.id] }).status,
    'requires-order-review',
  );
  assert.equal(commands.length, count);
  assert.equal(snapshot(), beforeRemoval);
  const removed = edit({ target: added.target, mode: 'remove', layerIds: [front.id] });
  assert.equal(removed.status, 'changed');
  assert.equal(commands.at(-1).type, 'remove-layers-from-folder');
  assert.deepEqual(
    commands.at(-1).payload.expectedPaintOrder,
    seed.layers.map((l) => l.id),
  );
  assert.deepEqual(owner.snapshot().layers, seed.layers);
  assert.deepEqual(owner.snapshot().editor.layerFolders[0].members, [
    { layerId: back.id },
    { layerId: middle.id },
  ]);
  const after = snapshot();
  owner.dispatch({ ...owner.state, type: 'undo' });
  assert.equal(snapshot(), beforeRemoval);
  owner.dispatch({ ...owner.state, type: 'redo' });
  assert.equal(snapshot(), after);
});
