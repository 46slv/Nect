import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import { deserialize, evaluate, exportSVG } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import {
  createLiveBlendModeOperation,
  createLiveBlendStepsOperation,
  createReadLiveBlendSettings,
} from '../src/live-blend-distance.js';
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

test('read-only Blend settings expose only one active control, without touching history', () => {
  const owner = ownerFor('live-blend'),
    target = () => targetFor(owner),
    addr = addressed(owner);
  const read = createReadLiveBlendSettings({ readTarget: target, getOwner: () => owner });
  const protocol = createLiveEditorProtocol({ readTarget: target, readBlendSettings: read });
  const args = { target: target(), ...addr };
  const packet = { version: 1, operation: 'blend.settings', arguments: args };
  const orig = snapshot(owner);
  assert.equal(
    createLiveEditorProtocol({ readTarget: target })(packet).status,
    'unsupported-operation',
  );
  assert.equal(
    protocol({ ...packet, arguments: { ...args, key: 'a' } }).status,
    'invalid-arguments',
  );
  assert.deepEqual(protocol(packet), {
    status: 'ok',
    mode: 'steps',
    spacing: { kind: 'steps', control: 'literal', value: 7, editable: true, maxSteps: 254 },
  });
  assert.equal(snapshot(owner), orig);
  assert.deepEqual(target(), args.target);
  owner.dispatch({
    ...owner.state,
    type: 'set-parameter',
    payload: { ...addr, key: 'steps', value: 3 },
  });
  assert.equal(protocol(packet).status, 'stale-target');
  assert.equal(protocol({ ...packet, arguments: { ...args, target: target() } }).spacing.value, 3);
});

test('Blend settings distinguish a driven value from its unused saved literal', () => {
  const owner = ownerFor('blend-distance'),
    addr = addressed(owner),
    target = targetFor(owner);
  const doc = owner.snapshot(),
    layer = doc.layers[0],
    blend = layer.nodes.find((n) => n.id === addr.nodeId);
  const read = createReadLiveBlendSettings({
    readTarget: () => target,
    getOwner: () => ({ snapshot: () => doc }),
  });
  const args = { ...addr, target };
  assert.equal(read(args).spacing.kind, 'distance');
  blend.params.distance.expression = 'unpublished expression';
  assert.deepEqual(read(args), {
    status: 'ok',
    mode: 'distance',
    spacing: {
      kind: 'distance',
      control: 'expression',
      value: null,
      editable: false,
      maxSteps: null,
    },
  });
  blend.params.distance.source = 'unpublished source';
  assert.equal(read(args).spacing.control, 'connection');
  assert.equal(read(args).spacing.value, null);
  blend.operation = 'smooth-color';
  assert.deepEqual(read(args), { status: 'ok', mode: 'smooth-color', spacing: null });
  blend.operation = 'unrecognized';
  assert.deepEqual(read(args), { status: 'unsupported-target' });
});

test('Blend mode is a closed, target-bound shared edit that retains authored input wires', () => {
  const live = ownerFor('blend-distance'),
    gui = ownerFor('blend-distance'),
    addr = addressed(live);
  const target = () => targetFor(live);
  let commits = 0,
    blocked = false;
  const setBlendMode = createLiveBlendModeOperation({
    readTarget: target,
    getOwner: () => live,
    isEditBlocked: () => blocked,
    commit(owner, command) {
      commits++;
      return owner.dispatch(command);
    },
  });
  const run = createLiveEditorProtocol({ readTarget: target, setBlendMode });
  const args = { ...addr, target: target(), mode: 'steps' },
    before = snapshot(live);
  const initial = live.snapshot().layers[0].nodes.find((n) => n.id === addr.nodeId);
  const call = (patch = {}) =>
    run({ version: 1, operation: 'blend.setMode', arguments: { ...args, ...patch } });
  assert.equal(call({ mode: 'bogus' }).status, 'invalid-arguments');
  assert.equal(call({ mode: 'subtract' }).status, 'invalid-arguments');
  assert.equal(call({ layerId: 'missing' }).status, 'unsupported-target');
  assert.equal(call({ mode: 'distance' }).status, 'unchanged');
  blocked = true;
  assert.equal(call().status, 'edit-in-progress');
  blocked = false;
  assert.equal(commits, 0);
  assert.equal(call().status, 'changed');
  assert.equal(call().status, 'stale-target');
  assert.equal(commits, 1);
  const result = live.snapshot().layers[0].nodes.find((n) => n.id === addr.nodeId);
  assert.equal(result.operation, 'steps');
  for (const id of ['a', 'b']) assert.equal(result.params[id].source, initial.params[id].source);
  gui.dispatch({ ...gui.state, type: 'set-operation', payload: { ...addr, operation: 'steps' } });
  assert.equal(exportSVG(live.snapshot()), exportSVG(gui.snapshot()));
  const after = snapshot(live);
  assert.equal(after, snapshot(gui));
  live.dispatch({ ...live.state, type: 'undo', payload: {} });
  assert.equal(snapshot(live), before);
  live.dispatch({ ...live.state, type: 'redo', payload: {} });
  assert.equal(snapshot(live), after);
});
