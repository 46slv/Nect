import assert from 'node:assert/strict';
import test from 'node:test';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { deserialize, serialize } from '../src/core.js';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';
import { createLiveLayerFolderRename } from '../src/live-layer-folder-rename.js';
import { orderingDocument } from './fixtures/artwork-ordering.js';

test('folder rename binds an exact identity and name to the current Session', () => {
  const seed = orderingDocument();
  const [layer] = seed.layers;
  seed.editor.layerFolders = [
    { id: 'named-folder', name: 'Before', members: [{ layerId: layer.id }] },
  ];
  const owner = createCompositionSessionOwner(
    deserializeCompositionProject(serialize(seed), deserialize),
  );
  let generation = 1;
  const readTarget = () => ({
    documentGeneration: generation,
    compositionId: owner.activeCompositionId,
    sessionId: owner.state.sessionId,
    sessionRevision: owner.state.expectedRevision,
    projectRevision: owner.projectRevision,
  });
  let blocked = false;
  let writes = 0;
  const host = {
    readTarget,
    getOwner: () => owner,
    isEditBlocked: () => blocked,
    commit(active, command) {
      assert.equal(active, owner);
      assert.equal(command.type, 'rename-layer-folder');
      writes++;
      return active.dispatch(command);
    },
  };
  const handler = createLiveEditorProtocol({
    readTarget,
    renameLayerFolder: createLiveLayerFolderRename(host),
  });
  const initial = readTarget();
  const req = {
    version: 1,
    operation: 'layerFolders.rename',
    arguments: { target: initial, folderId: 'named-folder', expectedName: 'Before', name: 'After' },
  };
  const edit = (args = {}) => handler({ ...req, arguments: { ...req.arguments, ...args } });
  const snapshot = () => serializeCompositionProject(owner.projectSnapshot());
  const before = snapshot();
  assert.equal(createLiveEditorProtocol({ readTarget })(req).status, 'unsupported-operation');
  assert.equal(edit({ path: 'bad' }).status, 'invalid-arguments');
  assert.equal(edit({ name: '   ' }).status, 'invalid-arguments');
  assert.equal(edit({ folderId: 'absent' }).status, 'unsupported-target');
  assert.equal(edit({ expectedName: 'wrong' }).status, 'name-conflict');
  assert.deepEqual(edit({ name: '  Before ' }), { status: 'unchanged', target: initial });
  blocked = true;
  assert.equal(edit().status, 'edit-in-progress');
  blocked = false;
  generation++;
  assert.equal(edit().status, 'stale-target');
  assert.equal(writes, 0);
  assert.equal(snapshot(), before);
  const target = readTarget();
  const result = edit({ target });
  assert.deepEqual(result, { status: 'changed', target: readTarget() });
  assert.equal(result.target.projectRevision, target.projectRevision + 1);
  assert.equal(edit({ target }).status, 'stale-target');
  assert.equal(writes, 1);
  assert.deepEqual(owner.snapshot().editor.layerFolders, [
    { id: 'named-folder', name: 'After', members: [{ layerId: layer.id }] },
  ]);
  assert.deepEqual(owner.snapshot().layers, seed.layers);
  const after = snapshot();
  owner.dispatch({ ...owner.state, type: 'undo' });
  assert.equal(snapshot(), before);
  owner.dispatch({ ...owner.state, type: 'redo' });
  assert.equal(snapshot(), after);
  assert.deepEqual(
    deserializeCompositionProject(after, deserialize).compositions[0].editor.layerFolders,
    owner.snapshot().editor.layerFolders,
  );
});
