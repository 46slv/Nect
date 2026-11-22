import assert from 'node:assert/strict';
import test from 'node:test';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { deserialize, serialize } from '../src/core.js';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';
import { createLiveLayerFolderDissolve } from '../src/live-layer-folder-dissolve.js';
import { orderingDocument } from './fixtures/artwork-ordering.js';

test('dissolve promotes nested metadata without deleting, rewriting or reordering artwork', () => {
  const seed = orderingDocument();
  const [back, middle, front] = seed.layers;
  front.locked = true;
  seed.editor.layerFolders = [
    { id: 'parent', name: 'Parent', members: [] },
    { id: 'middle-folder', name: 'Group', parentId: 'parent', members: [{ layerId: back.id }] },
    { id: 'child', name: 'Child', parentId: 'middle-folder', members: [{ layerId: middle.id }] },
    { id: 'locked', name: 'Locked', members: [{ layerId: front.id }] },
  ];
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
  let writes = 0;
  const handler = createLiveEditorProtocol({
    readTarget,
    dissolveLayerFolder: createLiveLayerFolderDissolve({
      readTarget,
      getOwner: () => owner,
      isEditBlocked: () => blocked,
      commit(active, command) {
        assert.equal(active, owner);
        assert.equal(command.type, 'dissolve-layer-folder');
        writes++;
        try {
          return active.dispatch(command);
        } catch (error) {
          assert.match(error.message, /Unlock selected Layers first/);
          return null;
        }
      },
    }),
  });
  const initial = readTarget();
  const request = {
    version: 1,
    operation: 'layerFolders.dissolve',
    arguments: { target: initial, folderId: 'middle-folder' },
  };
  const edit = (args = {}) => handler({ ...request, arguments: { ...request.arguments, ...args } });
  assert.equal(createLiveEditorProtocol({ readTarget })(request).status, 'unsupported-operation');
  assert.equal(edit({ deleteChildren: true }).status, 'invalid-arguments');
  assert.equal(edit({ folderId: 'absent' }).status, 'unsupported-target');
  blocked = true;
  assert.equal(edit().status, 'edit-in-progress');
  blocked = false;
  assert.equal(writes, 0);
  const before = snapshot();
  assert.equal(edit({ folderId: 'locked' }).status, 'rejected');
  assert.equal(snapshot(), before);
  assert.deepEqual(readTarget(), initial);
  const changed = edit();
  assert.equal(changed.status, 'changed');
  assert.equal(changed.target.projectRevision, initial.projectRevision + 1);
  assert.equal(edit().status, 'stale-target');
  assert.deepEqual(owner.snapshot().layers, seed.layers);
  assert.deepEqual(owner.snapshot().editor.layerFolders, [
    { id: 'parent', name: 'Parent', members: [{ layerId: back.id }] },
    { id: 'child', name: 'Child', parentId: 'parent', members: [{ layerId: middle.id }] },
    { id: 'locked', name: 'Locked', members: [{ layerId: front.id }] },
  ]);
  const after = snapshot();
  owner.dispatch({ ...owner.state, type: 'undo' });
  assert.equal(snapshot(), before);
  owner.dispatch({ ...owner.state, type: 'redo' });
  assert.equal(snapshot(), after);
});
