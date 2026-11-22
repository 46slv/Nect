import assert from 'node:assert/strict';
import test from 'node:test';
import { deserialize, exportSVG, serialize } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';
import { createLiveLayerReorder } from '../src/live-layer-reorder.js';

import { orderingDocument } from './fixtures/artwork-ordering.js';

const sample = orderingDocument();
sample.layers[1].locked = true;
const seed = serialize(sample);
const makeOwner = () =>
  createCompositionSessionOwner(deserializeCompositionProject(seed, deserialize));
const snapshot = (owner) => serializeCompositionProject(owner.projectSnapshot());

test('opt-in live reorder matches GUI paint order, history and persisted document', () => {
  const live = makeOwner();
  const gui = makeOwner();
  const readTarget = () => ({
    documentGeneration: 1,
    compositionId: live.activeCompositionId,
    sessionId: live.state.sessionId,
    sessionRevision: live.state.expectedRevision,
    projectRevision: live.projectRevision,
  });
  let blocked = false;
  let writes = 0;
  const reorderLayer = createLiveLayerReorder({
    readTarget,
    getOwner: () => live,
    isEditBlocked: () => blocked,
    commit(owner, command) {
      writes++;
      return owner.dispatch(command);
    },
  });
  const handle = createLiveEditorProtocol({ readTarget, reorderLayer });
  const [first, locked, ...rest] = live.snapshot().layers;
  assert.equal(locked.locked, true);
  const before = snapshot(live);
  const beforeSvg = exportSVG(live.snapshot());
  const target = readTarget();
  const request = {
    version: 1,
    operation: 'layer.reorder',
    arguments: { target, layerId: first.id, beforeLayerId: null },
  };
  const edit = (override) =>
    handle({ ...request, arguments: { ...request.arguments, ...override } });
  assert.deepEqual(createLiveEditorProtocol({ readTarget })(request), {
    status: 'unsupported-operation',
  });
  assert.equal(edit({ index: 2 }).status, 'invalid-arguments');
  assert.equal(edit({ beforeLayerId: 0 }).status, 'invalid-arguments');
  assert.equal(edit({ beforeLayerId: 'missing' }).status, 'unsupported-target');
  assert.equal(edit({ layerId: 'missing' }).status, 'unsupported-target');
  assert.equal(edit({ layerId: locked.id }).status, 'rejected');
  assert.equal(edit({ beforeLayerId: first.id }).status, 'unchanged');
  assert.equal(edit({ beforeLayerId: locked.id }).status, 'unchanged');
  blocked = true;
  assert.equal(handle(request).status, 'edit-in-progress');
  blocked = false;
  assert.equal(writes, 0);
  assert.equal(snapshot(live), before);
  assert.deepEqual(readTarget(), target);

  gui.dispatch({
    ...gui.state,
    type: 'reorder-layer',
    payload: { layerId: first.id, beforeLayerId: null },
  });
  assert.deepEqual(handle(request), { status: 'changed', target: readTarget() });
  assert.equal(writes, 1);
  const after = snapshot(live);
  assert.equal(after, snapshot(gui));
  assert.deepEqual(live.snapshot().layers, [locked, ...rest, first]);
  assert.notEqual(exportSVG(live.snapshot()), beforeSvg);
  assert.equal(exportSVG(live.snapshot()), exportSVG(gui.snapshot()));
  assert.equal(handle(request).status, 'stale-target');
  assert.equal(edit({ target: readTarget() }).status, 'unchanged');
  assert.equal(writes, 1);

  live.dispatch({ ...live.state, type: 'undo' });
  assert.equal(snapshot(live), before);
  live.dispatch({ ...live.state, type: 'redo' });
  assert.equal(snapshot(live), after);
  const reopened = createCompositionSessionOwner(deserializeCompositionProject(after, deserialize));
  assert.equal(snapshot(reopened), after);
  assert.equal(edit({ target: readTarget(), beforeLayerId: locked.id }).status, 'changed');
  assert.equal(snapshot(live), before);
  assert.equal(writes, 2);

  const send = (type, payload) => live.dispatch({ ...live.state, type, payload });
  send('set-layer-state', { layerId: locked.id, key: 'locked', value: false });
  const { folderId } = send('create-layer-folder', {
    name: 'Pair',
    layerIds: [locked.id, rest[0].id],
  });
  send('rename-layer-folder', { folderId, name: 'Redo candidate' });
  send('undo');
  const grouped = snapshot(live);
  const groupedTarget = readTarget();
  assert.equal(edit({ target: groupedTarget, beforeLayerId: rest[0].id }).status, 'rejected');
  assert.equal(writes, 2);
  assert.equal(snapshot(live), grouped);
  assert.deepEqual(readTarget(), groupedTarget);
  send('redo');
  assert.equal(live.snapshot().editor.layerFolders[0].name, 'Redo candidate');
});
