import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import { deserialize, evaluate, exportSVG } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createLiveBlendStepsOperation } from '../src/live-blend-distance.js';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';

const ownerFor = (name) =>
  createCompositionSessionOwner(
    deserializeCompositionProject(
      readFileSync(new URL('../examples/' + name + '.json', import.meta.url), 'utf8'),
      deserialize,
    ),
  );
const targetFor = (owner) => ({
  documentGeneration: 1,
  compositionId: owner.activeCompositionId,
  sessionId: owner.state.sessionId,
  sessionRevision: owner.state.expectedRevision,
  projectRevision: owner.projectRevision,
});
const addressed = (owner) => {
  const layer = owner.snapshot().layers[0];
  return { layerId: layer.id, nodeId: layer.nodes.find((n) => n.type === 'Blend').id };
};
const snapshot = (owner) => serializeCompositionProject(owner.projectSnapshot());

test('Blend Steps edits use one GUI command and preserve history, geometry and copy', () => {
  const live = ownerFor('live-blend'),
    gui = ownerFor('live-blend');
  const target = () => targetFor(live);
  let blocked = false,
    commits = 0;
  const setBlendSteps = createLiveBlendStepsOperation({
    readTarget: target,
    getOwner: () => live,
    isEditBlocked: () => blocked,
    commit(owner, command) {
      commits++;
      return owner.dispatch(command);
    },
  });
  const protocol = createLiveEditorProtocol({ readTarget: target, setBlendSteps });
  const addr = addressed(live);
  const args = { target: target(), ...addr, value: 3 };
  const packet = { version: 1, operation: 'blend.setSteps', arguments: args };
  const call = (change = {}) => protocol({ ...packet, arguments: { ...args, ...change } });
  const before = snapshot(live),
    count = () => {
      const doc = live.snapshot();
      return evaluate(doc.layers[0], {}, doc.imageAssets, doc).path.children.length;
    };
  assert.equal(count(), 9);
  assert.equal(
    createLiveEditorProtocol({ readTarget: target })(packet).status,
    'unsupported-operation',
  );
  for (const bad of [-1, 3.5, 255, NaN])
    assert.equal(call({ value: bad }).status, 'invalid-arguments');
  assert.equal(call({ key: 'a' }).status, 'invalid-arguments');
  blocked = true;
  assert.equal(call().status, 'edit-in-progress');
  blocked = false;
  assert.equal(call({ nodeId: 'missing' }).status, 'unsupported-target');
  assert.equal(commits, 0);
  assert.equal(snapshot(live), before);
  gui.dispatch({
    ...gui.state,
    type: 'set-parameter',
    payload: { ...addr, key: 'steps', value: 3 },
  });
  assert.equal(call().status, 'changed');
  assert.equal(commits, 1);
  assert.equal(count(), 5);
  assert.equal(call({ value: 4 }).status, 'stale-target');
  assert.equal(commits, 1);
  const after = snapshot(live);
  assert.equal(after, snapshot(gui));
  assert.equal(exportSVG(live.snapshot()), exportSVG(gui.snapshot()));
  const reopened = createCompositionSessionOwner(deserializeCompositionProject(after, deserialize));
  assert.equal(exportSVG(reopened.snapshot()), exportSVG(live.snapshot()));
  live.dispatch({ ...live.state, type: 'undo', payload: {} });
  assert.equal(snapshot(live), before);
  live.dispatch({ ...live.state, type: 'redo', payload: {} });
  assert.equal(snapshot(live), after);
});

test('Blend Steps cannot write an inactive or driven parameter, or exceed the three-source cap', () => {
  const owner = ownerFor('blend-distance'),
    target = targetFor(owner),
    addr = addressed(owner);
  const call = (layerPatch, nodePatch, value = 3) => {
    const state = owner.snapshot(),
      layer = state.layers.find((l) => l.id === addr.layerId);
    const n = layer.nodes.find((n) => n.id === addr.nodeId);
    n.params.steps = { type: 'Scalar', value: 5 };
    Object.assign(layer, layerPatch);
    Object.assign(n, nodePatch);
    const set = createLiveBlendStepsOperation({
      readTarget: () => target,
      getOwner: () => ({ snapshot: () => state }),
      isEditBlocked: () => false,
      commit() {
        assert.fail('rejected request must not dispatch');
      },
    });
    return set({ target, ...addr, value }).status;
  };
  assert.equal(call({}, {}), 'unsupported-target');
  assert.equal(call({ locked: true }, { operation: 'steps' }), 'unsupported-target');
  assert.equal(call({}, { operation: 'steps', bypassed: true }), 'unsupported-target');
  assert.equal(
    call({}, { operation: 'steps', params: { steps: { type: 'Scalar', expression: '2+2' } } }),
    'unsupported-target',
  );
  assert.equal(call({}, { operation: 'three-source-steps' }, 127), 'invalid-arguments');
});
