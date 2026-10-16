import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, document, node, point, evaluate, guideDefaults } from '../src/core.js';

function setup(doc = document()) {
  const session = new CommandSession(doc);
  const send = (type, payload) => session.dispatch({ ...session.state, type, payload });
  return { session, send, begin: (target) => send('begin-gesture', { target }) };
}

test('multiple generator movement validates every fixed target and commits all positions once', () => {
  const doc = document(),
    layer = doc.layers[0],
    circle = layer.nodes[0],
    curve = node('Curve');
  layer.nodes.push(curve);
  const { session: s, send, begin } = setup(doc),
    before = s.snapshot();
  let token = begin({ kind: 'nodes-translate', layerId: layer.id, nodeIds: [circle.id, curve.id] });
  token = send('update-gesture', { ...token, update: { delta: [20, -10], nodeIds: ['foreign'] } });
  assert.deepEqual(s.snapshot(), before);
  const preview = s.previewSnapshot(token.gestureId).document;
  assert.deepEqual(
    preview.layers[0].nodes[0].params.center.value,
    circle.params.center.value.map((v, i) => v + [20, -10][i]),
  );
  assert.deepEqual(
    preview.layers[0].nodes.at(-1).points[0].position.value,
    curve.points[0].position.value.map((v, i) => v + [20, -10][i]),
  );
  send('commit-gesture', token);
  assert.equal(s.historyView.past.length, 1);
  send('undo');
  assert.deepEqual(s.snapshot(), before);
  const guarded = s.snapshot();
  guarded.layers[0].nodes.at(-1).points[1].position.locked = true;
  const locked = setup(guarded);
  assert.throws(
    () =>
      locked.begin({ kind: 'nodes-translate', layerId: layer.id, nodeIds: [circle.id, curve.id] }),
    /locked/,
  );
  assert.deepEqual(locked.session.snapshot(), guarded);
  assert.equal(s.historyView.future.length, 1);
});

test('Curve handle coupling is private, respects driven opposite handles and keeps origin identity', () => {
  const doc = document(),
    layer = doc.layers[0],
    curve = node('Curve'),
    pt = point(100, 100);
  pt.handleType = 'symmetric';
  pt.handleB.value = [10, 5];
  pt.handleA.value = [-10, -5];
  curve.points = [pt, point(200, 100), point(200, 200)];
  layer.nodes.push(curve);
  const { session: s, send, begin } = setup(doc);
  const target = {
    kind: 'curve-point',
    layerId: layer.id,
    nodeId: curve.id,
    pointId: pt.id,
    key: 'handleB',
  };
  const original = s.historyView.doc;
  let token = begin(target);
  token = send('update-gesture', { ...token, update: { delta: [5, 7] } });
  const previewPoint = s.previewSnapshot(token.gestureId).document.layers[0].nodes.at(-1).points[0];
  assert.deepEqual(previewPoint.handleB.value, [15, 12]);
  assert.deepEqual(previewPoint.handleA.value, [-15, -12]);
  token = send('update-gesture', { ...token, update: { delta: [0, 0] } });
  send('commit-gesture', token);
  assert.equal(s.historyView.doc, original);
  send('set-parameter-lock', {
    layerId: layer.id,
    nodeId: curve.id,
    key: `${pt.id}.handleA`,
    locked: true,
  });
  token = begin(target);
  token = send('update-gesture', { ...token, update: { delta: [5, 7] } });
  send('commit-gesture', token);
  assert.deepEqual(s.snapshot().layers[0].nodes.at(-1).points[0].handleA.value, [-10, -5]);
});

test('Remap allocation stays private, zero/cancel preserves sparse records, stale cancellation preserves new work', () => {
  const doc = document(),
    layer = doc.layers[0],
    remap = layer.nodes[3],
    id = evaluate(layer).run(remap.params.path.source).contours[0].points[0].id;
  const { session: s, send, begin } = setup(doc),
    before = s.snapshot(),
    original = s.historyView.doc;
  const target = {
    kind: 'remap-point',
    layerId: layer.id,
    nodeId: remap.id,
    pointId: id,
    key: 'position',
  };
  let token = begin(target);
  assert.deepEqual(s.snapshot(), before);
  token = send('update-gesture', { ...token, update: { delta: [11, 6] } });
  assert.equal(s.snapshot().layers[0].nodes[3].edits.length, 0);
  assert.deepEqual(
    s.previewSnapshot(token.gestureId).document.layers[0].nodes[3].edits[0].params.position.value,
    [11, 6],
  );
  token = send('update-gesture', { ...token, update: { delta: [0, 0] } });
  send('commit-gesture', token);
  assert.equal(s.historyView.doc, original);
  token = begin(target);
  token = send('update-gesture', { ...token, update: { delta: [11, 6] } });
  assert.throws(() => {
    s.historyView.doc.name = 'New authored work';
  }, TypeError);
  assert.throws(
    () => send('rename', { layerId: layer.id, name: 'Interleaved write' }),
    /GESTURE_ACTIVE/,
  );
  send('cancel-gesture', token);
  assert.equal(s.snapshot().name, before.name);
  assert.equal(s.snapshot().layers[0].nodes[3].edits.length, 0);
  token = begin(target);
  token = send('update-gesture', { ...token, update: { delta: [11, 6] } });
  send('commit-gesture', token);
  assert.equal(s.historyView.past.length, 1);
});

test('guide preview uses the same typed command and refuses locks without changing history', () => {
  const doc = document();
  doc.editor = {
    ...guideDefaults(),
    guides: [{ id: 'g', axis: 'x', position: 50, locked: false }],
  };
  const { session: s, send, begin } = setup(doc);
  let token = begin({ kind: 'guide-position', guideId: 'g' });
  token = send('update-gesture', { ...token, update: { value: 80 } });
  assert.equal(s.snapshot().editor.guides[0].position, 50);
  assert.equal(s.previewSnapshot(token.gestureId).document.editor.guides[0].position, 80);
  assert.throws(() => send('update-gesture', { ...token, update: { value: Infinity } }));
  send('commit-gesture', token);
  send('undo');
  send('set-guide', { guideId: 'g', key: 'locked', value: true });
  assert.throws(() => begin({ kind: 'guide-position', guideId: 'g' }), /locked/);
});
