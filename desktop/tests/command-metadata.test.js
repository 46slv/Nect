import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, document, node, serialize, deserialize } from '../src/core.js';

function fixture() {
  const doc = document(),
    layer = doc.layers[0],
    a = node('Vector Math'),
    b = node('Vector Math'),
    ref = node('Reference');
  b.params.a.value = [37, 38];
  b.params.a.source = a.id;
  layer.nodes.push(a, b, ref);
  const session = new CommandSession(doc);
  const dispatch = (type, payload = {}) =>
    session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } });
  return { session, dispatch, layer, a, b, ref };
}

test('identity and metadata share History, detached settings, no-op redo and serialized stable IDs', () => {
  const { session: s, dispatch, layer, a } = fixture();
  const initial = s.snapshot();
  dispatch('rename', { nodeId: a.id, name: '  数値 A  ', expectedName: 'Vector Math' });
  const renamed = s.snapshot();
  dispatch('undo');
  assert.deepEqual(s.snapshot(), initial);
  const revision = s.state.expectedRevision;
  dispatch('rename', { nodeId: a.id, name: 'Vector Math' });
  assert.equal(s.state.expectedRevision, revision);
  dispatch('redo');
  assert.deepEqual(s.snapshot(), renamed);
  const settings = { collapsed: true };
  dispatch('set-node-settings', { nodeId: a.id, settings });
  settings.collapsed = false;
  dispatch('rename', { name: 'Foreground' });
  dispatch('set-layer-state', { key: 'guide', value: true });
  dispatch('set-document-settings', { key: 'width', value: 1200 });
  const loaded = deserialize(serialize(s.snapshot()));
  assert.equal(loaded.layers[0].id, layer.id);
  assert.equal(loaded.layers[0].nodes.find((n) => n.id === a.id).collapsed, true);
  assert.equal(loaded.layers[0].name, 'Foreground');
  assert.equal(loaded.layers[0].guide, true);
  assert.equal(loaded.width, 1200);
});

test('delete disconnects exact IDs while preserving fallback and Reference identity; Undo restores graph', () => {
  const { session: s, dispatch, a, b, ref } = fixture();
  dispatch('set-reference', { nodeId: ref.id, target: { nodeId: a.id, portId: 'output' } });
  const before = s.snapshot();
  dispatch('delete-node', { nodeId: a.id });
  const layer = s.snapshot().layers[0];
  assert.equal(layer.nodes.find((n) => n.id === b.id).params.a.source, undefined);
  assert.deepEqual(layer.nodes.find((n) => n.id === b.id).params.a.value, [37, 38]);
  assert.equal(layer.nodes.find((n) => n.id === ref.id).reference.nodeId, a.id);
  dispatch('undo');
  assert.deepEqual(s.snapshot(), before);
  dispatch('set-node-bypassed', { nodeId: b.id, bypassed: true });
  assert.deepEqual(s.snapshot().layers[0].nodes.find((n) => n.id === b.id).params, b.params);
});

test('creation and preview allocate in core and commit one History entry', () => {
  const { session: s, dispatch } = fixture();
  const created = dispatch('create-node', { nodeType: 'Rectangle', position: [70, 80] });
  const beforePreview = s.snapshot(),
    count = s.historyView.past.length;
  assert.deepEqual(
    beforePreview.layers[0].nodes.find((n) => n.id === created.nodeId).position,
    [70, 80],
  );
  dispatch('preview-node', { nodeId: created.nodeId, position: [400, 80] });
  const l = s.snapshot().layers[0],
    out = l.nodes.find((n) => n.id === l.output),
    paint = l.nodes.find((n) => n.id === out.params.path.source);
  assert.equal(paint.type, 'Fill');
  assert.equal(paint.params.path.source, created.nodeId);
  assert.equal(s.historyView.past.length, count + 1);
  dispatch('undo');
  assert.deepEqual(s.snapshot(), beforePreview);
});

test('invalid late preview, cycles, stale names, unknown fields, types and output deletion preserve redo', () => {
  const { session: s, dispatch, layer, a, b, ref } = fixture();
  dispatch('set-reference', { nodeId: ref.id, target: { nodeId: a.id, portId: 'output' } });
  dispatch('rename', { name: 'Temporary' });
  dispatch('undo');
  const before = s.snapshot(),
    state = s.state,
    future = s.historyView.future.length;
  const locked = structuredClone(before);
  locked.layers[0].nodes.find((n) => n.id === layer.output).params.path.locked = true;
  const guarded = new CommandSession(locked);
  assert.throws(
    () =>
      guarded.dispatch({
        ...guarded.state,
        type: 'preview-node',
        payload: { layerId: layer.id, nodeId: layer.nodes[0].id },
      }),
    /locked/,
  );
  assert.deepEqual(guarded.snapshot(), locked, 'failed preview must not leave its new Fill');
  for (const [type, payload] of [
    ['rename', { nodeId: a.id, name: 'X', expectedName: 'Foreign' }],
    ['set-node-settings', { nodeId: a.id, settings: { params: {} } }],
    ['set-node-settings', { nodeId: a.id, settings: { collapsed: 'yes' } }],
    ['set-node-settings', { nodeId: a.id, settings: { strokeCap: 'round' } }],
    ['set-layer-state', { key: 'visible', value: 0 }],
    ['set-document-settings', { key: 'height', value: -1 }],
    ['delete-node', { nodeId: layer.output }],
    ['delete-node', { nodeId: 'missing' }],
    ['create-node', { nodeType: 'Foreign' }],
    ['create-node', { nodeType: 'Math', position: [Infinity, 1] }],
    ['set-node-bypassed', { nodeId: layer.nodes[0].id, bypassed: true }],
  ]) {
    assert.throws(() => dispatch(type, payload), type);
    assert.deepEqual(s.snapshot(), before, type);
    assert.deepEqual(s.state, state, type);
    assert.equal(s.historyView.future.length, future, type);
  }
  assert.throws(() =>
    dispatch('set-reference', { nodeId: ref.id, target: { nodeId: ref.id, portId: 'output' } }),
  );
  dispatch('redo');
  assert.equal(s.snapshot().layers[0].name, 'Temporary');
});

test('locked layer permits visibility and unlock, but refuses graph, rename and guide changes', () => {
  const { session: s, dispatch, a } = fixture();
  dispatch('set-layer-state', { key: 'locked', value: true });
  const state = s.state;
  for (const [type, payload] of [
    ['rename', { name: 'X' }],
    ['create-node', { nodeType: 'Math' }],
    ['delete-node', { nodeId: a.id }],
    ['set-layer-state', { key: 'guide', value: true }],
  ]) {
    assert.throws(() => dispatch(type, payload), /locked/);
    assert.deepEqual(s.state, state);
  }
  dispatch('set-layer-state', { key: 'visible', value: false });
  dispatch('set-layer-state', { key: 'locked', value: false });
  dispatch('rename', { name: 'Unlocked' });
  assert.equal(s.snapshot().layers[0].name, 'Unlocked');
  assert.throws(
    () => s.dispatch({ ...state, type: 'rename', payload: { name: 'Stale' } }),
    /STALE_REVISION/,
  );
});

test('guide commands retain manual identities through layout replacement, locks and failed edits', () => {
  const { session: s, dispatch } = fixture();
  const { guideId } = dispatch('create-guide', { axis: 'x' });
  dispatch('set-layout-grid', {
    config: { columns: 2, rows: 2, marginX: 20, marginY: 20, gutterX: 10, gutterY: 10 },
  });
  dispatch('set-guide', { guideId, key: 'locked', value: true });
  const before = s.snapshot(),
    state = s.state;
  for (const [type, payload] of [
    ['delete-guide', { guideId }],
    ['set-guide', { guideId, key: 'position', value: 42 }],
    ['set-guide-setting', { key: 'guides', value: [] }],
    ['set-guide-setting', { key: 'snap', value: 'yes' }],
  ]) {
    assert.throws(() => dispatch(type, payload));
    assert.deepEqual(s.snapshot(), before);
    assert.deepEqual(s.state, state);
  }
  dispatch('set-layout-grid', { config: null });
  assert.deepEqual(s.snapshot().editor.guides, [
    before.editor.guides.find((g) => g.id === guideId),
  ]);
  dispatch('undo');
  assert.deepEqual(s.snapshot(), before);
});

test('point and Remap commands retain stable point IDs and roll back failed late targets', () => {
  const { session: s, dispatch } = fixture();
  const { nodeId: curveId } = dispatch('create-node', { nodeType: 'Curve' });
  const { nodeId: remapId } = dispatch('create-node', { nodeType: 'Path Remap' });
  dispatch('connect', { sourceId: curveId, nodeId: remapId, key: 'path' });
  const points = s.snapshot().layers[0].nodes.find((n) => n.id === curveId).points;
  dispatch('set-point-mode', { nodeId: curveId, pointId: points[0].id, mode: 'symmetric' });
  dispatch('edit-remap', {
    nodeId: remapId,
    action: 'translate',
    pointIds: [points[0].id],
    delta: [5, 6],
  });
  const before = s.snapshot(),
    state = s.state;
  assert.throws(() =>
    dispatch('edit-remap', {
      nodeId: remapId,
      action: 'translate',
      pointIds: [points[0].id, 'foreign'],
      delta: [7, 8],
    }),
  );
  assert.deepEqual(s.snapshot(), before);
  assert.deepEqual(s.state, state);
  dispatch('delete-point', { nodeId: curveId, pointId: points[1].id });
  assert.equal(
    s.snapshot().layers[0].nodes.find((n) => n.id === curveId).points[1].id,
    points[2].id,
  );
  dispatch('undo');
  assert.deepEqual(s.snapshot(), before);
});
