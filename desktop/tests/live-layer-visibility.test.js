import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import { deserialize, exportSVG } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createLiveLayerLock, createLiveLayerVisibility } from '../src/live-layer-visibility.js';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';
import { readLiveEditorObjects } from '../src/live-editor-objects.js';

const seed = readFileSync(new URL('../examples/blend-distance.json', import.meta.url), 'utf8');
const makeOwner = () =>
  createCompositionSessionOwner(deserializeCompositionProject(seed, deserialize));
const snapshot = (owner) => serializeCompositionProject(owner.projectSnapshot());
const svg = (owner) => exportSVG(owner.snapshot());

test('explicit layer visibility matches the GUI command, export and undo/redo', () => {
  const live = makeOwner();
  const gui = makeOwner();
  const readTarget = () => ({
    documentGeneration: 1,
    compositionId: live.activeCompositionId,
    sessionId: live.state.sessionId,
    sessionRevision: live.state.expectedRevision,
    projectRevision: live.projectRevision,
  });
  let commits = 0;
  let blocked = false;
  const set = createLiveLayerVisibility({
    readTarget,
    getOwner: () => live,
    isEditBlocked: () => blocked,
    commit(owner, command) {
      commits++;
      return owner.dispatch(command);
    },
  });
  const { id: layerId } = live.snapshot().layers[0];
  const before = snapshot(live);
  const visibleSvg = svg(live);
  const target = readTarget();
  const request = { target, layerId, visible: false };
  assert.deepEqual(set({ ...request, visible: true }), { status: 'unchanged' });
  assert.deepEqual(set({ ...request, visible: 0 }), { status: 'invalid-arguments' });
  assert.deepEqual(set({ ...request, layerId: 'not-a-layer' }), { status: 'unsupported-target' });
  blocked = true;
  assert.deepEqual(set(request), { status: 'edit-in-progress' });
  blocked = false;
  assert.equal(commits, 0);
  assert.equal(snapshot(live), before);

  gui.dispatch({
    ...gui.state,
    type: 'set-layer-state',
    payload: { layerId, key: 'visible', value: false },
  });
  assert.deepEqual(set(request), { status: 'changed' });
  assert.equal(commits, 1);
  const after = snapshot(live);
  assert.equal(after, snapshot(gui));
  assert.notEqual(svg(live), visibleSvg);
  assert.equal(svg(live), svg(gui));
  assert.deepEqual(set(request), { status: 'stale-target' });
  assert.deepEqual(set({ ...request, target: readTarget() }), { status: 'unchanged' });
  assert.equal(commits, 1);
  live.dispatch({ ...live.state, type: 'undo' });
  assert.equal(snapshot(live), before);
  assert.equal(svg(live), visibleSvg);
  live.dispatch({ ...live.state, type: 'redo' });
  assert.equal(snapshot(live), after);
  const reopened = createCompositionSessionOwner(deserializeCompositionProject(after, deserialize));
  assert.equal(svg(reopened), svg(live));
});

test('hiding a locked layer preserves its lock, other layers and stored visibility under Solo', () => {
  const project = readFileSync(
    new URL('../examples/artwork-bounds-snapping.json', import.meta.url),
    'utf8',
  );
  const live = createCompositionSessionOwner(deserializeCompositionProject(project, deserialize));
  const gui = createCompositionSessionOwner(deserializeCompositionProject(project, deserialize));
  const [first, locked] = live.snapshot().layers;
  assert.equal(locked.locked, true);
  assert.equal(locked.visible, true);
  for (const owner of [live, gui])
    owner.dispatch({
      ...owner.state,
      type: 'set-layer-state',
      payload: { layerId: first.id, key: 'solo', value: true },
    });

  const readTarget = () => ({
    documentGeneration: 1,
    compositionId: live.activeCompositionId,
    sessionId: live.state.sessionId,
    sessionRevision: live.state.expectedRevision,
    projectRevision: live.projectRevision,
  });
  const before = snapshot(live);
  const initial = live.snapshot().layers;
  const list = () =>
    readLiveEditorObjects(
      { readTarget, readDocument: () => live.snapshot() },
      { target: readTarget(), layerIndex: null, offset: 0 },
    ).items;
  assert.equal(list().find((layer) => layer.layerId === locked.id).visible, true);
  assert.equal(snapshot(live), before);
  const set = createLiveLayerVisibility({
    readTarget,
    getOwner: () => live,
    isEditBlocked: () => false,
    commit: (owner, command) => owner.dispatch(command),
  });
  const old = readTarget();
  assert.deepEqual(set({ target: old, layerId: locked.id, visible: false }), { status: 'changed' });
  assert.deepEqual(set({ target: old, layerId: locked.id, visible: true }), {
    status: 'stale-target',
  });
  assert.deepEqual(
    list().find((layer) => layer.layerId === locked.id),
    {
      layerId: locked.id,
      layerIndex: 1,
      locked: true,
      visible: false,
    },
  );
  const current = live.snapshot().layers;
  assert.deepEqual(
    current,
    initial.map((layer) => (layer.id === locked.id ? { ...layer, visible: false } : layer)),
  );
  gui.dispatch({
    ...gui.state,
    type: 'set-layer-state',
    payload: { layerId: locked.id, key: 'visible', value: false },
  });
  assert.equal(snapshot(live), snapshot(gui));
  assert.equal(svg(live), svg(gui));
  live.dispatch({ ...live.state, type: 'undo' });
  assert.equal(snapshot(live), before);
});
test('explicit opt-in layer lock matches GUI history and never unlocks a stale target', () => {
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
  const setLayerLock = createLiveLayerLock({
    readTarget,
    getOwner: () => live,
    isEditBlocked: () => blocked,
    commit(owner, command) {
      writes++;
      return owner.dispatch(command);
    },
  });
  const handle = createLiveEditorProtocol({ readTarget, setLayerLock });
  const first = live.snapshot().layers[0];
  assert.equal(first.locked, false);
  const before = snapshot(live);
  const beforeSvg = svg(live);
  const request = {
    version: 1,
    operation: 'layer.setLock',
    arguments: { target: readTarget(), layerId: first.id, locked: true },
  };
  assert.deepEqual(createLiveEditorProtocol({ readTarget })(request), {
    status: 'unsupported-operation',
  });
  const edit = (overrides) =>
    handle({ ...request, arguments: { ...request.arguments, ...overrides } });
  assert.deepEqual(edit({ key: 'solo' }), { status: 'invalid-arguments' });
  assert.deepEqual(edit({ locked: 0 }), { status: 'invalid-arguments' });
  assert.deepEqual(edit({ layerId: 'missing' }), { status: 'unsupported-target' });
  blocked = true;
  assert.deepEqual(handle(request), { status: 'edit-in-progress' });
  blocked = false;
  assert.deepEqual(edit({ locked: false }), { status: 'unchanged', target: readTarget() });
  assert.equal(writes, 0);
  assert.equal(snapshot(live), before);

  gui.dispatch({
    ...gui.state,
    type: 'set-layer-state',
    payload: { layerId: first.id, key: 'locked', value: true },
  });
  assert.deepEqual(handle(request), { status: 'changed', target: readTarget() });
  const after = snapshot(live);
  assert.equal(after, snapshot(gui));
  assert.equal(svg(live), beforeSvg);
  assert.deepEqual(edit({ locked: false }), { status: 'stale-target' });
  assert.deepEqual(edit({ target: readTarget() }), { status: 'unchanged', target: readTarget() });
  assert.equal(writes, 1);
  assert.equal(snapshot(live), after);

  live.dispatch({ ...live.state, type: 'undo' });
  assert.equal(snapshot(live), before);
  live.dispatch({ ...live.state, type: 'redo' });
  assert.equal(snapshot(live), after);
  const reopened = createCompositionSessionOwner(deserializeCompositionProject(after, deserialize));
  assert.equal(reopened.snapshot().layers[0].locked, true);
  assert.deepEqual(edit({ target: readTarget(), locked: false }), {
    status: 'changed',
    target: readTarget(),
  });
  assert.equal(snapshot(live), before);
  assert.equal(writes, 2);
});
