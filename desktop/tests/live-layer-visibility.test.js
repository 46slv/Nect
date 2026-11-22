import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import { deserialize, exportSVG } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createLiveLayerVisibility } from '../src/live-layer-visibility.js';

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
