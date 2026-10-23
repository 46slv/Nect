import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, connect, node, serialize } from '../src/core.js';
import { directSelectionSource } from '../src/direct-selection.js';
import { pointAlignmentContext } from '../src/point-alignment.js';
import { selectedPointBounds, selectedPointArtboardBounds } from '../src/selected-point-bounds.js';
import { directMarqueeFixture } from './fixtures/direct-anchor-marquee.js';
import { compoundFixture } from './fixtures/contour-navigation.js';

const query = (f, ids) =>
  selectedPointArtboardBounds(f.doc, f.layer.id, (f.curve || f.path).id, ids);
const extrema = (positions) => {
  const x = positions.map((p) => p[0]),
    y = positions.map((p) => p[1]);
  return {
    minX: Math.min(...x),
    maxX: Math.max(...x),
    width: Math.max(...x) - Math.min(...x),
    minY: Math.min(...y),
    maxY: Math.max(...y),
    height: Math.max(...y) - Math.min(...y),
  };
};
// Independent scalar formula, not the production affine/route helpers.
const project = (xy, transform) => {
  const q = transform.params,
    x = (xy[0] - q.anchor.value[0]) * q.scaleWidth.value,
    y = (xy[1] - q.anchor.value[1]) * q.scale.value,
    radians = (q.rotation.value * Math.PI) / 180;
  return [
    x * Math.cos(radians) - y * Math.sin(radians) + q.position.value[0] + q.offset.value[0],
    x * Math.sin(radians) + y * Math.cos(radians) + q.position.value[1] + q.offset.value[1],
  ];
};
const nearValues = (actual, expected) => {
  for (const key of Object.keys(expected))
    assert.ok(
      Math.abs(actual[key] - expected[key]) <= 1e-10 * Math.max(1, Math.abs(expected[key])),
      `${key}: ${actual[key]} != ${expected[key]}`,
    );
};

test('rotated nonuniform Transform measures selected points, not transformed local bounds corners', () => {
  const f = directMarqueeFixture({ transformed: true }),
    ids = f.curve.points.slice(0, 3).map((p) => p.id),
    before = serialize(f.doc),
    expected = extrema(
      f.curve.points.slice(0, 3).map((p) => project(p.position.value, f.transform)),
    ),
    summary = query(f, ids);
  assert.equal(summary.state, 'ready');
  assert.equal(summary.count, 3);
  nearValues(summary.values, expected);
  nearValues(
    summary.values,
    extrema(
      pointAlignmentContext(f.doc, f.layer.id, f.curve.id, ids).points.map((p) => p.position),
    ),
  );
  assert.notDeepEqual(summary.values, selectedPointBounds(f.curve, ids).values);
  assert.equal(serialize(f.doc), before);
  assert.deepEqual(query(f, new Set([...ids].reverse())), summary);
  const one = query(f, [ids[0]]);
  assert.equal(one.state, 'ready');
  assert.equal(one.values.width, 0);
  assert.equal(one.values.height, 0);
});

test('all selected IDs across compound contours survive storage reordering and chained Transforms', () => {
  const f = compoundFixture([8, 5, 7, 4]),
    a = node('Transform'),
    b = node('Transform'),
    ids = [
      f.path.contours[0].points[1],
      f.path.contours[1].points[3],
      f.path.contours[3].points[2],
    ];
  for (const [n, anchor, position, sx, sy, angle] of [
    [a, [45, -20], [80, 110], 1.7, 0.6, 37],
    [b, [-10, 35], [60, 50], -0.9, 1.3, -21],
  ]) {
    n.params.anchor.value = anchor;
    n.params.position = { type: 'Vector2', value: position, expression: '', locked: false };
    n.params.uniformScale = { type: 'Boolean', value: false, expression: '', locked: false };
    n.params.scaleWidth = { type: 'Scalar', value: sx, expression: '', locked: false };
    n.params.scale.value = sy;
    n.params.rotation.value = angle;
  }
  f.layer.nodes.splice(1, 0, a, b);
  connect(f.layer, f.path.id, a.id, 'path');
  connect(f.layer, a.id, b.id, 'path');
  connect(f.layer, b.id, f.fill.id, 'path');
  const positions = ids.map((id) => f.path.points.find((p) => p.id === id).position.value),
    expected = extrema(positions.map((p) => project(project(p, a), b))),
    summary = query(f, ids);
  assert.equal(summary.state, 'ready');
  nearValues(summary.values, expected);
  f.path.points.reverse();
  assert.deepEqual(query(f, [...ids].reverse()), summary);
  for (const point of f.path.points) {
    point.handleA.value = [-999, 555];
    point.handleB.value = [777, -444];
  }
  f.fill.params.color.value = { hex: '#ff0000', alpha: 0.3 };
  assert.deepEqual(query(f, ids), summary);
});

test('identity route retains tiny/exact coordinates and extreme maxima without cancellation', () => {
  const f = directMarqueeFixture(),
    ids = f.curve.points.slice(0, 2).map((p) => p.id);
  f.curve.points[0].position.value = [1e-20, -1e-20];
  f.curve.points[1].position.value = [2e-20, -1e-20];
  assert.deepEqual(query(f, ids).values, {
    minX: 1e-20,
    maxX: 2e-20,
    width: 1e-20,
    minY: -1e-20,
    maxY: -1e-20,
    height: 0,
  });
  f.curve.points[0].position.value = [-1e16, 0];
  f.curve.points[1].position.value = [1, 0];
  assert.equal(query(f, ids).values.maxX, 1);
  assert.deepEqual(query(f, []).state, 'empty');
  f.curve.points[0].position.value = [-Number.MAX_VALUE, 0];
  f.curve.points[1].position.value = [Number.MAX_VALUE, 0];
  assert.deepEqual(query(f, ids), {
    state: 'unavailable',
    count: 2,
    reason: 'Bounds out of range',
  });
});

test('selected anchors across many point-pane pages are all measured', () => {
  const f = directMarqueeFixture(),
    template = f.curve.points[0];
  f.curve.points = Array.from({ length: 256 }, (_, i) => ({
    ...structuredClone(template),
    id: `many-${i}`,
    position: { ...structuredClone(template.position), value: [i - 128, i / 10] },
  }));
  const summary = query(
    f,
    f.curve.points.map((p) => p.id),
  );
  assert.equal(summary.count, 256);
  assert.deepEqual(summary.values, {
    minX: -128,
    maxX: 127,
    width: 255,
    minY: 0,
    maxY: 25.5,
    height: 25.5,
  });
});

test('unsupported, stale, locked and driven routes expose reasons without partial values or mutations', async (t) => {
  for (const [name, change, reason] of [
    [
      'hidden layer',
      (f) => {
        f.layer.visible = false;
      },
      /visible artwork/,
    ],
    [
      'guide layer',
      (f) => {
        f.layer.guide = true;
      },
      /visible artwork/,
    ],
    [
      'locked layer',
      (f) => {
        f.layer.locked = true;
      },
      /Locked selection/,
    ],
    [
      'locked Transform',
      (f) => {
        f.transform.locked = true;
      },
      /Point route is locked/,
    ],
    [
      'singular Transform',
      (f) => {
        f.transform.params.scale.value = 0;
      },
      /nonsingular Transform/,
    ],
    [
      'near-zero Transform',
      (f) => {
        f.transform.params.scale.value = 1e-13;
      },
      /nonsingular Transform/,
    ],
    [
      'driven Transform',
      (f) => {
        f.transform.params.rotation.expression = 'value';
      },
      /driven Transform/,
    ],
    [
      'bypassed route',
      (f) => {
        f.fill.bypassed = true;
      },
      /Restore the path route/,
    ],
    [
      'unsupported route',
      (f) => {
        const repeat = node('Geometry Repeater');
        f.layer.nodes.push(repeat);
        connect(f.layer, f.transform.id, repeat.id, 'path');
        connect(f.layer, repeat.id, f.fill.id, 'path');
      },
      /generated or combined/,
    ],
    [
      'different output source',
      (f) => {
        const other = node('Curve');
        f.layer.nodes.push(other);
        connect(f.layer, other.id, f.fill.id, 'path');
      },
      /outside this direct path route/,
    ],
    [
      'locked point',
      (f) => {
        f.curve.points[0].position.locked = true;
      },
      /Locked position/,
    ],
    [
      'driven point',
      (f) => {
        f.curve.points[0].position.expression = 'value; value';
      },
      /Driven position/,
    ],
    [
      'missing selected point',
      (f, ids) => {
        ids.push('missing');
      },
      /Selection changed/,
    ],
    [
      'duplicate selected point',
      (f, ids) => {
        ids.push(ids[0]);
      },
      /Selection changed/,
    ],
    [
      'missing owner',
      (f) => {
        f.curve = { id: 'missing' };
      },
      /Owned points only/,
    ],
  ])
    await t.test(name, () => {
      const f = directMarqueeFixture({ transformed: true }),
        ids = f.curve.points.slice(0, 3).map((p) => p.id);
      change(f, ids);
      const before = structuredClone(f),
        summary = query(f, ids);
      assert.equal(summary.state, 'unavailable');
      assert.match(summary.reason, reason);
      assert.equal(Object.hasOwn(summary, 'values'), false);
      assert.deepEqual(f, before);
      const route = directSelectionSource(f.doc, f.layer.id, { sourceId: f.curve.id });
      if (
        !route.supported &&
        !/Owned|Selection changed|Locked position|Driven position/.test(summary.reason)
      )
        assert.equal(summary.reason, route.reason);
    });
});

test('read-only queries preserve Session document/revision/history and follow Transform Undo/Redo', () => {
  const f = directMarqueeFixture({ transformed: true }),
    session = new CommandSession(f.doc),
    ids = f.curve.points.slice(0, 3).map((p) => p.id),
    read = () => selectedPointArtboardBounds(session.snapshot(), f.layer.id, f.curve.id, ids),
    snapshot = () => ({
      doc: serialize(session.snapshot()),
      state: session.state,
      history: session.historyView,
    });
  const original = read(),
    before = snapshot();
  for (let i = 0; i < 3; i++) assert.deepEqual(read(), original);
  assert.deepEqual(snapshot(), before);
  session.dispatch({
    ...session.state,
    type: 'set-transform-control',
    payload: { layerId: f.layer.id, nodeId: f.transform.id, key: 'rotation', value: 63 },
  });
  const changed = read(),
    after = snapshot();
  assert.notDeepEqual(changed.values, original.values);
  read();
  assert.deepEqual(snapshot(), after);
  session.dispatch({ ...session.state, type: 'undo' });
  assert.deepEqual(read(), original);
  session.dispatch({ ...session.state, type: 'redo' });
  assert.deepEqual(read(), changed);
});
