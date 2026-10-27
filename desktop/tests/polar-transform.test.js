import test from 'node:test';
import assert from 'node:assert/strict';
import { polarSettings, polarTransformGeometry } from '../src/polar-transform.js';
import {
  blankDocument,
  node,
  connect,
  evaluate,
  validate,
  serialize,
  deserialize,
  exportSVG,
  CommandSession,
} from '../src/core.js';

const settings = {
  origin: [0, 0],
  center: [10, 20],
  width: 400,
  height: 200,
  radius: 100,
  angle: 0,
  inverse: false,
};
const path = (positions, closed = false) => ({
  contours: [
    {
      id: 'contour',
      closed,
      points: positions.map((position, i) => ({
        id: `p${i}`,
        position,
        handleA: [0, 0],
        handleB: [0, 0],
      })),
    },
  ],
  fill: { hex: '#123456', alpha: 0.4 },
});
const near = (a, b) => a.forEach((v, i) => assert.ok(Math.abs(v - b[i]) < 1e-8, `${a} != ${b}`));

test('rectangular X maps clockwise around the output center and Y sets radius', () => {
  const source = path([
      [0, 200],
      [100, 200],
      [200, 200],
      [300, 200],
      [400, 200],
    ]),
    before = structuredClone(source);
  const actual = polarTransformGeometry(source, settings);
  actual.contours[0].points.forEach((p, i) =>
    near(
      p.position,
      [
        [110, 20],
        [10, 120],
        [-90, 20],
        [10, -80],
        [110, 20],
      ][i],
    ),
  );
  assert.deepEqual(source, before);
  assert.deepEqual(actual.fill, source.fill);
  assert.notEqual(actual.fill, source.fill);
  assert.deepEqual(
    actual.contours[0].points.map((p) => p.id),
    source.contours[0].points.map((p) => p.id),
  );
});

test('angle, origin and zero radius point use documented coordinates', () => {
  const actual = polarTransformGeometry(
    path([
      [50, 100],
      [50, 300],
    ]),
    { ...settings, origin: [50, 100], angle: -90 },
  );
  near(actual.contours[0].points[0].position, [10, 20]);
  near(actual.contours[0].points[1].position, [10, -80]);
});

test('absolute handles are mapped then restored as offsets', () => {
  const source = path([[0, 100]]);
  source.contours[0].points[0].handleB = [100, 0];
  const p = polarTransformGeometry(source, settings).contours[0].points[0];
  near(p.position, [60, 20]);
  near(p.handleB, [-50, 50]);
  near(p.handleA, [0, 0]);
});

test('inverse returns anchors away from seam; center has deterministic angle zero', () => {
  const source = path([
    [80, 80],
    [120, 100],
  ]);
  const polar = polarTransformGeometry(source, settings);
  const restored = polarTransformGeometry(polar, { ...settings, inverse: true });
  restored.contours[0].points.forEach((p, i) =>
    near(p.position, source.contours[0].points[i].position),
  );
  near(
    polarTransformGeometry(path([[10, 20]]), { ...settings, inverse: true }).contours[0].points[0]
      .position,
    [0, 0],
  );
});

test('inverse refuses seam crossing rather than drawing a long false connector', () => {
  assert.throws(
    () =>
      polarTransformGeometry(
        path([
          [110, 19],
          [110, 21],
        ]),
        { ...settings, inverse: true },
      ),
    /seam/,
  );
});

test('invalid options, negative radii and over-budget geometry reject without mutation', () => {
  for (const width of [0, -1, Infinity, NaN, 100001])
    assert.throws(() => polarSettings({ ...settings, width }));
  assert.throws(() => polarSettings({ ...settings, inverse: 1 }));
  assert.throws(() => polarTransformGeometry(path([[0, -1]]), settings), /negative radii/);
  const source = path([[0, 1]]);
  source.contours[0].points[0].handleB[0] = Infinity;
  assert.throws(() => polarTransformGeometry(source, settings), /finite/);
  assert.throws(
    () => polarTransformGeometry({ contours: Array(10001).fill({}) }, settings),
    /budget/,
  );
});

test('core graph evaluates, preserves editable parameters and exports vector paths', () => {
  const doc = blankDocument(),
    l = doc.layers[0],
    rectangle = node('Rectangle'),
    polar = node('Polar Transform'),
    fill = node('Fill'),
    out = l.nodes[0];
  rectangle.params.center.value = [480, 270];
  rectangle.params.size.value = [400, 120];
  l.nodes.unshift(rectangle, polar, fill);
  connect(l, rectangle.id, polar.id, 'path');
  connect(l, polar.id, fill.id, 'path');
  connect(l, fill.id, out.id, 'path');
  validate(doc);
  const result = evaluate(l, {}, {}, doc);
  assert.equal(result.error, null);
  assert.ok(result.path.contours.length);
  assert.equal(serialize(deserialize(serialize(doc))), serialize(doc));
  assert.match(exportSVG(doc), /<path /);
  const session = new CommandSession(doc),
    before = serialize(session.snapshot());
  assert.throws(() =>
    session.dispatch({
      ...session.state,
      type: 'set-parameter',
      payload: { layerId: l.id, nodeId: polar.id, key: 'width', value: 0 },
    }),
  );
  assert.equal(serialize(session.snapshot()), before);
  session.dispatch({
    ...session.state,
    type: 'set-parameter',
    payload: { layerId: l.id, nodeId: polar.id, key: 'angle', value: 45 },
  });
  const after = serialize(session.snapshot());
  assert.notEqual(after, before);
  session.dispatch({ ...session.state, type: 'undo' });
  assert.equal(serialize(session.snapshot()), before);
  session.dispatch({ ...session.state, type: 'redo' });
  assert.equal(serialize(session.snapshot()), after);
});
