import assert from 'node:assert/strict';
import test from 'node:test';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { deserialize, serialize } from '../src/core.js';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';
import { readLiveLayerFolders } from '../src/live-editor-objects.js';
import { createLiveLayerFolder } from '../src/live-layer-folder-create.js';
import { orderingDocument } from './fixtures/artwork-ordering.js';

test('folder creation uses explicit selection, revision guards and shared Session history', () => {
  const seed = orderingDocument();
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
  const createLayerFolder = createLiveLayerFolder({
    readTarget,
    getOwner: () => owner,
    isEditBlocked: () => blocked,
    commit(active, command) {
      assert.equal(active, owner);
      commands.push(command);
      try {
        return active.dispatch(command);
      } catch (error) {
        assert.match(error.message, /Select adjacent Layers and Folders/);
        return null;
      }
    },
  });
  const handler = createLiveEditorProtocol({ readTarget, createLayerFolder });
  const [back, middle, front] = owner.snapshot().layers;
  const originalTarget = readTarget();
  const original = snapshot();
  const request = {
    version: 1,
    operation: 'layerFolders.create',
    arguments: {
      target: originalTarget,
      name: 'Group',
      layerIds: [back.id, middle.id],
      folderIds: [],
    },
  };
  const edit = (args = {}) => handler({ ...request, arguments: { ...request.arguments, ...args } });
  assert.equal(createLiveEditorProtocol({ readTarget })(request).status, 'unsupported-operation');
  assert.equal(edit({ folderId: 'caller-picked' }).status, 'invalid-arguments');
  assert.equal(edit({ layerIds: [back.id, back.id] }).status, 'invalid-arguments');
  assert.equal(
    edit({
      layerIds: ['a'.repeat(256)].concat(Array.from({ length: 14 }, (_, i) => i + 'b'.repeat(255))),
    }).status,
    'invalid-arguments',
  );
  blocked = true;
  assert.equal(edit().status, 'edit-in-progress');
  blocked = false;
  assert.equal(commands.length, 0);
  assert.equal(snapshot(), original);
  const bad = edit({ layerIds: [back.id, front.id] });
  assert.equal(bad.status, 'rejected');
  assert.equal(snapshot(), original);
  assert.deepEqual(readTarget(), originalTarget);
  const group = edit();
  assert.equal(group.status, 'changed');
  assert.deepEqual(group.target, readTarget());
  assert.equal(group.target.projectRevision, originalTarget.projectRevision + 1);
  assert.equal(commands.at(-1).type, 'create-layer-folder');
  assert.equal(edit().status, 'stale-target');
  assert.deepEqual(owner.snapshot().layers, seed.layers);
  const parent = edit({
    target: group.target,
    name: 'Parent',
    layerIds: [],
    folderIds: [group.folderId],
  });
  assert.equal(parent.status, 'changed');
  const topo = readLiveLayerFolders(
    { readTarget, readDocument: () => owner.snapshot() },
    { target: parent.target, offset: 0 },
  );
  assert.equal(topo.status, 'ok');
  assert.deepEqual(topo.items, [
    { kind: 'folder', folderId: group.folderId, parentFolderId: parent.folderId },
    { kind: 'membership', folderId: group.folderId, layerId: back.id },
    { kind: 'membership', folderId: group.folderId, layerId: middle.id },
    { kind: 'folder', folderId: parent.folderId, parentFolderId: null },
  ]);
  const final = snapshot();
  owner.dispatch({ ...owner.state, type: 'undo' });
  assert.equal(owner.snapshot().editor.layerFolders.length, 1);
  owner.dispatch({ ...owner.state, type: 'redo' });
  assert.equal(snapshot(), final);
});

test('missing creation receipt after a committed change is unknown and not replayed', () => {
  const owner = createCompositionSessionOwner(
    deserializeCompositionProject(serialize(orderingDocument()), deserialize),
  );
  const readTarget = () => ({
    documentGeneration: 1,
    compositionId: owner.activeCompositionId,
    sessionId: owner.state.sessionId,
    sessionRevision: owner.state.expectedRevision,
    projectRevision: owner.projectRevision,
  });
  let calls = 0;
  const handler = createLiveEditorProtocol({
    readTarget,
    createLayerFolder: createLiveLayerFolder({
      readTarget,
      getOwner: () => owner,
      isEditBlocked: () => false,
      commit(owner, command) {
        calls++;
        owner.dispatch(command);
        return null;
      },
    }),
  });
  const request = {
    version: 1,
    operation: 'layerFolders.create',
    arguments: { target: readTarget(), name: 'Empty', layerIds: [], folderIds: [] },
  };
  assert.equal(handler(request).status, 'outcome-unknown');
  assert.equal(handler(request).status, 'stale-target');
  assert.equal(calls, 1);
  assert.equal(owner.snapshot().editor.layerFolders.length, 1);
});
