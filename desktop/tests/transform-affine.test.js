import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  connect,
  evaluate,
  CommandSession,
  serialize,
  deserialize,
  exportSVG,
  parameter,
} from '../src/core.js';
import { transformValues, transformVector } from '../src/transform-affine.js';
import { hitPointControl, pointControlTargets } from '../src/point-controls.js';
const near = (a, b) => a.forEach((v, i) => assert.ok(Math.abs(v - b[i]) < 1e-7, `${a} != ${b}`));
function fixture() {
  const doc = document(),
    l = doc.layers[0],
    curve = node('Curve'),
    tr = node('Transform'),
    fill = node('Fill'),
    out = node('Output');
  l.nodes = [curve, tr, fill, out];
  l.output = out.id;
  connect(l, curve.id, tr.id, 'path');
  connect(l, tr.id, fill.id, 'path');
  connect(l, fill.id, out.id, 'path');
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: l.id, nodeId: tr.id, ...payload },
    });
  return {
    doc,
    l,
    curve,
    tr,
    session,
    send,
    read: () => session.snapshot().layers[0].nodes.find((n) => n.id === tr.id),
  };
}
test('old Transform JSON, authored drivers and origin semantics remain unchanged on load', () => {
  const f = fixture();
  f.tr.params.offset.value = [3, 4];
  f.tr.params.anchor.expression = '5;5';
  f.tr.params.scale.expression = '2';
  f.tr.params.rotation.value = 90;
  f.tr.params.anchor.locked = true;
  const before = serialize(f.doc),
    path = evaluate(f.l).path;
  const restored = deserialize(before);
  assert.equal(serialize(restored), before);
  assert.deepEqual(evaluate(restored.layers[0]).path, path);
  near(path.contours[0].points[0].position, [-282, 499]);
});
test('Skew at zero rotation shares affine geometry, handles and character provenance', () => {
  const f = fixture();
  const original = f.session.snapshot();
  f.send('set-transform-control', { key: 'skew', value: 30 });
  const doc = f.session.snapshot(),
    l = doc.layers[0],
    r = evaluate(l),
    p = r.run(f.tr.id).contours[0].points[0];
  near(p.position, [250 - Math.tan(Math.PI / 6) * 150, 150]);
  assert.equal(doc.schemaVersion, original.schemaVersion);
  assert.equal(doc.layers[0].nodes.find((n) => n.id === f.tr.id).params.rotation.value, 0);
  assert.match(exportSVG(doc), /<svg/);
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0]).path, r.path);
  f.send('undo');
  assert.deepEqual(f.session.snapshot(), original);
  f.send('redo');
  assert.deepEqual(f.session.snapshot(), doc);
});
test('absolute Position, Anchor and independent percentages retain legacy offset and scalar scale', () => {
  const f = fixture();
  f.send('set-parameter', { key: 'offset', value: [3, 4] });
  f.send('set-transform-control', { key: 'position', value: [100, 200] });
  f.send('set-transform-control', { key: 'anchor', value: [10, 20] });
  f.send('set-transform-control', { key: 'uniformScale', value: false });
  f.send('set-transform-control', { key: 'scaleWidth', value: 2 });
  f.send('set-transform-control', { key: 'scale', value: 3 });
  const r = evaluate(f.session.snapshot().layers[0]),
    state = r.transformValues(f.tr.id);
  near(transformVector(state, [10, 20]), [103, 204]);
  near(transformVector(state, [11, 21]), [105, 207]);
  f.send('set-transform-control', { key: 'uniformScale', value: true });
  assert.equal(f.read().params.scaleWidth.value, 2);
  near(
    transformVector(evaluate(f.session.snapshot().layers[0]).transformValues(f.tr.id), [11, 21]),
    [106, 207],
  );
});
test('invalid skew, locked/driven edits and no-op keep authored state and redo atomic', () => {
  const f = fixture();
  f.send('set-transform-control', { key: 'skew', value: 20 });
  f.send('undo');
  const before = f.session.snapshot(),
    state = f.session.state;
  f.send('set-transform-control', { key: 'skew', value: 0 });
  assert.deepEqual(f.session.state, state);
  assert.deepEqual(f.session.snapshot(), before);
  assert.throws(() => f.send('set-transform-control', { key: 'skew', value: 90 }), /Skew/);
  assert.deepEqual(f.session.snapshot(), before);
  f.send('redo');
  f.send('set-parameter-expression', { key: 'scale', expression: '2' });
  const driven = f.session.snapshot();
  assert.throws(() => f.send('set-transform-control', { key: 'scale', value: 3 }), /driven/);
  assert.deepEqual(f.session.snapshot(), driven);
});
test('Vector Transform uses same point affine without changing existing typed Path sockets', () => {
  const doc = document(),
    l = doc.layers[0],
    v = node('Vector'),
    t = node('Vector Transform');
  doc.schemaVersion = 11;
  l.nodes.push(v, t);
  connect(l, v.id, t.id, 'vector');
  const session = new CommandSession(doc);
  session.dispatch({
    ...session.state,
    type: 'set-transform-control',
    payload: { layerId: l.id, nodeId: t.id, key: 'skew', value: 30 },
  });
  const d = session.snapshot(),
    r = evaluate(d.layers[0]);
  near(r.run(t.id), [300 - Math.tan(Math.PI / 6) * 250, 250]);
  assert.throws(
    () => connect(l, v.id, l.nodes.find((n) => n.type === 'Path Remap').id, 'path'),
    /Cannot connect/,
  );
});
test('Skew Axis turns the shear plane and screen hit testing picks nearest dense anchor', () => {
  const n = node('Transform');
  n.params.skew = { ...parameter(n, 'skew'), value: 45 };
  n.params.skewAxis = { ...parameter(n, 'skewAxis'), value: 90 };
  const state = transformValues(n, (k) => parameter(n, k).value);
  near(transformVector(state, [10, 20]), [10, 30]);
  const path = {
    contours: [
      {
        points: [
          { id: 'a', position: [10, 10], handleA: [0, 0], handleB: [10, 0] },
          { id: 'b', position: [12, 10], handleA: [0, 0], handleB: [0, 0] },
        ],
      },
    ],
  };
  assert.equal(pointControlTargets(path, 1).length, 2);
  assert.equal(pointControlTargets(path, 1, ['a']).length, 3);
  assert.equal(hitPointControl(path, [11.8, 10], 1).pointId, 'b');
  assert.equal(hitPointControl(path, [12, 11], 8), null);
  path.contours[0].points[1].position = [10, 10];
  assert.equal(hitPointControl(path, [10, 10], 1).ambiguous, true);
});

test('new Position and Width controls do not inherit legacy drivers', () => {
  const f = fixture();
  f.send('set-parameter-expression', { key: 'anchor', expression: '10;20' });
  f.send('set-parameter-expression', { key: 'scale', expression: '2' });
  f.send('set-transform-control', { key: 'position', axis: 0, value: 100 });
  assert.deepEqual(f.read().params.position.value, [100, 20]);
  assert.equal(f.read().params.position.expression, '');
  assert.equal(f.read().params.anchor.expression, '10;20');
  f.send('set-transform-control', { key: 'uniformScale', value: false });
  f.send('set-transform-control', { key: 'scaleWidth', value: 3 });
  assert.equal(f.read().params.scaleWidth.value, 3);
  assert.equal(f.read().params.scaleWidth.expression, '');
  assert.equal(f.read().params.scale.expression, '2');
  const state = evaluate(f.session.snapshot().layers[0]).transformValues(f.tr.id);
  assert.equal(state.scale, 2);
  assert.equal(state.scaleWidth, 3);
});
