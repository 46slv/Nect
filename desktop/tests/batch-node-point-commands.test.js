import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, document, node, connect, serialize } from '../src/core.js';
function setup() {
  const doc = document(),
    l = doc.layers[0],
    curve = node('Curve'),
    a = node('Value'),
    b = node('Math');
  l.nodes.push(curve, a, b);
  connect(l, a.id, b.id, 'a');
  const session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({ ...session.state, type, payload: { layerId: l.id, ...payload } });
  return { l, curve, a, b, session, send };
}
test('batch node deletion preserves Output, disconnects survivors and has a single undo', () => {
  const { l, curve, a, b, session, send } = setup(),
    before = serialize(session.snapshot());
  const result = send('delete-nodes', { nodeIds: [a.id, curve.id, l.output] });
  assert.deepEqual(result.retainedNodeIds, [l.output]);
  const after = session.snapshot().layers[0];
  assert.ok(after.nodes.some((n) => n.id === l.output));
  assert.ok(!after.nodes.some((n) => n.id === a.id || n.id === curve.id));
  assert.equal(after.nodes.find((n) => n.id === b.id).params.a.source, undefined);
  assert.equal(session.historyView.past.length, 1);
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  send('redo');
  assert.equal(session.historyView.past.length, 1);
  const revision = session.state.expectedRevision;
  send('delete-nodes', { nodeIds: [l.output] });
  assert.equal(session.state.expectedRevision, revision);
});
test('batch deletion rejects missing IDs atomically and single Output deletion remains protected', () => {
  const { l, a, session, send } = setup(),
    before = serialize(session.snapshot());
  assert.throws(() => send('delete-nodes', { nodeIds: [a.id, 'missing'] }), /Missing/);
  assert.throws(() => send('delete-node', { nodeId: l.output }), /Output/);
  assert.equal(serialize(session.snapshot()), before);
});
test('pointIds gesture translates selected anchors together with one undo and unchanged handles', () => {
  const { l, curve, session, send } = setup(),
    ids = curve.points.slice(0, 2).map((p) => p.id),
    before = session.snapshot();
  let token = send('begin-gesture', {
    target: {
      kind: 'curve-point',
      layerId: l.id,
      nodeId: curve.id,
      pointId: ids[0],
      pointIds: ids,
      key: 'position',
    },
  });
  token = send('update-gesture', { ...token, update: { delta: [12, -7] } });
  send('commit-gesture', token);
  const after = session.snapshot().layers[0].nodes.find((n) => n.id === curve.id);
  after.points.forEach((p, i) => {
    assert.deepEqual(
      p.position.value,
      curve.points[i].position.value.map((v, k) => v + (i < 2 ? [12, -7][k] : 0)),
    );
    assert.deepEqual(p.handleA, curve.points[i].handleA);
    assert.deepEqual(p.handleB, curve.points[i].handleB);
  });
  assert.equal(session.historyView.past.length, 1);
  send('undo');
  assert.deepEqual(session.snapshot(), before);
});
test('a locked point rejects the whole group before preview or history changes', () => {
  const { l, curve, session } = setup(),
    doc = session.snapshot();
  doc.layers[0].nodes.find((n) => n.id === curve.id).points[1].position.locked = true;
  const s = new CommandSession(doc),
    before = serialize(s.snapshot()),
    ids = curve.points.slice(0, 2).map((p) => p.id);
  assert.throws(
    () =>
      s.dispatch({
        ...s.state,
        type: 'begin-gesture',
        payload: {
          target: {
            kind: 'curve-point',
            layerId: l.id,
            nodeId: curve.id,
            pointId: ids[0],
            pointIds: ids,
            key: 'position',
          },
        },
      }),
    /locked/,
  );
  assert.equal(serialize(s.snapshot()), before);
  assert.equal(s.historyView.past.length, 0);
});
test('split scale captures evaluated linked X while retaining the Y source and locks', () => {
  const { l, a, session } = setup(),
    doc = session.snapshot(),
    transform = node('Transform');
  doc.layers[0].nodes.push(transform);
  doc.layers[0].nodes.find((n) => n.id === a.id).params.value.value = 2.5;
  connect(doc.layers[0], a.id, transform.id, 'scale');
  transform.params.scale.locked = true;
  const s = new CommandSession(doc);
  s.dispatch({
    ...s.state,
    type: 'set-transform-control',
    payload: { layerId: l.id, nodeId: transform.id, key: 'uniformScale', value: false },
  });
  const actual = s.snapshot().layers[0].nodes.find((n) => n.id === transform.id);
  assert.equal(actual.params.scaleWidth.value, 2.5);
  assert.equal(actual.params.scale.source, a.id);
  assert.equal(actual.params.scale.locked, true);
});
