import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, document, node, point, serialize, deserialize } from '../src/core.js';
import { handlePolar, polarHandleValue, commitHandlePolar } from '../src/handle-polar.js';

function fixture(type = 'Curve', mode = 'corner', guard = () => {}) {
  const doc = document(),
    layer = doc.layers[0],
    curve = node(type);
  curve.points = [point(100, 100), point(200, 100), point(200, 200)];
  if (type === 'Multi Path') curve.units = [{ id: 'unit-test', contours: ['contour-test'] }];
  if (type === 'Multi Path')
    curve.contours = [{ id: 'contour-test', points: curve.points.map((p) => p.id), closed: true }];
  const p = curve.points[0];
  p.handleA.value = [-30, 0];
  p.handleB.value = [50, 0];
  p.handleType = mode;
  doc.schemaVersion = 26;
  layer.nodes.push(curve);
  guard({ doc, layer, curve, p });
  const session = new CommandSession(doc);
  return {
    session,
    curve,
    p,
    target: {
      kind: 'curve-point',
      layerId: layer.id,
      nodeId: curve.id,
      pointId: p.id,
      key: 'handleB',
    },
    read: () => session.snapshot().layers[0].nodes.at(-1).points[0],
  };
}
const near = (a, b) => a.forEach((v, i) => assert.ok(Math.abs(v - b[i]) < 1e-9, `${a} != ${b}`));

test('polar conversion handles signed angles, zero and rejects invalid numbers', () => {
  assert.deepEqual(handlePolar([0, 0]), { angle: 0, length: 0 });
  assert.deepEqual(polarHandleValue([0, 0], 'length', 20), [20, 0]);
  assert.deepEqual(polarHandleValue([20, 0], 'length', 0), [0, 0]);
  assert.deepEqual(polarHandleValue([10, 0], 'angle', -90), [0, -10]);
  near(polarHandleValue([3, 4], 'length', 10), [6, 8]);
  for (const value of [-1, NaN, Infinity, -Infinity])
    assert.throws(() => polarHandleValue([10, 0], 'length', value));
  assert.throws(() => polarHandleValue([0, 0], 'angle', 90));
  assert.throws(() => handlePolar([Number.MAX_VALUE, Number.MAX_VALUE]));
});

for (const type of ['Curve', 'Multi Path'])
  for (const mode of ['corner', 'smooth', 'symmetric'])
    for (const key of ['handleA', 'handleB'])
      test(`${type} ${mode} ${key}: Canvas coupling, one undo/redo and JSON`, () => {
        const f = fixture(type, mode),
          target = { ...f.target, key },
          other = key === 'handleA' ? 'handleB' : 'handleA';
        const before = serialize(f.session.snapshot());
        commitHandlePolar(f.session, target, 'angle', 90);
        const edited = f.read();
        near(edited[key].value, [0, Math.hypot(...f.p[key].value)]);
        near(
          edited[other].value,
          mode === 'corner'
            ? f.p[other].value
            : [
                0,
                -(mode === 'symmetric'
                  ? Math.hypot(...f.p[key].value)
                  : Math.hypot(...f.p[other].value)),
              ],
        );
        assert.equal(f.session.historyView.past.length, 1);
        const after = serialize(f.session.snapshot());
        assert.equal(serialize(deserialize(after)), after);
        f.session.dispatch({ ...f.session.state, type: 'undo' });
        assert.equal(serialize(f.session.snapshot()), before);
        f.session.dispatch({ ...f.session.state, type: 'redo' });
        assert.equal(serialize(f.session.snapshot()), after);
        commitHandlePolar(f.session, target, 'length', 12);
        near(f.read()[key].value, [0, 12]);
        if (mode === 'symmetric') near(f.read()[other].value, [0, -12]);
      });

test('guard failures and invalid inputs preserve state, history and redo; opposite handles remain protected', () => {
  for (const guard of [
    ({ layer }) => (layer.locked = true),
    ({ curve }) => (curve.locked = true),
    ({ p }) => (p.handleB.locked = true),
    ({ p }) => (p.handleB.expression = 'value; value'),
  ]) {
    const f = fixture('Curve', 'symmetric', guard),
      before = serialize(f.session.snapshot());
    assert.throws(() => commitHandlePolar(f.session, f.target, 'length', 25));
    assert.equal(serialize(f.session.snapshot()), before);
    assert.equal(f.session.historyView.past.length, 0);
  }
  const f = fixture('Curve', 'smooth', ({ p }) => (p.handleA.locked = true));
  commitHandlePolar(f.session, f.target, 'angle', 90);
  assert.deepEqual(f.read().handleA.value, [-30, 0]);
  f.session.dispatch({ ...f.session.state, type: 'undo' });
  const before = serialize(f.session.snapshot());
  for (const value of [-1, NaN, Infinity])
    assert.throws(() => commitHandlePolar(f.session, f.target, 'length', value));
  assert.equal(serialize(f.session.snapshot()), before);
  assert.equal(f.session.historyView.future.length, 1);
  commitHandlePolar(f.session, f.target, 'length', 50);
  assert.equal(f.session.historyView.future.length, 1);
});

test('connected handle rejects polar input and a driven opposite handle is preserved', () => {
  for (const key of ['handleA', 'handleB']) {
    const f = fixture('Curve', 'symmetric', ({ layer, p }) => {
      const vector = node('Vector');
      vector.params.value.value = [8, 6];
      layer.nodes.unshift(vector);
      p[key].source = vector.id;
    });
    if (key === 'handleB')
      assert.throws(() => commitHandlePolar(f.session, f.target, 'length', 20));
    else {
      commitHandlePolar(f.session, f.target, 'length', 20);
      assert.equal(f.read().handleA.source, f.p.handleA.source);
      assert.deepEqual(f.read().handleA.value, [-30, 0]);
    }
  }
});

test('zero-length commits follow Canvas coupling without NaN and undo exactly', () => {
  for (const mode of ['corner', 'smooth', 'symmetric']) {
    const f = fixture('Curve', mode),
      before = serialize(f.session.snapshot());
    commitHandlePolar(f.session, f.target, 'length', 0);
    assert.deepEqual(f.read().handleB.value, [0, 0]);
    assert.ok(f.read().handleA.value.every(Number.isFinite));
    if (mode !== 'corner') near(f.read().handleA.value, [0, 0]);
    f.session.dispatch({ ...f.session.state, type: 'undo' });
    assert.equal(serialize(f.session.snapshot()), before);
    assert.equal(f.session.historyView.past.length, 0);
  }
});
