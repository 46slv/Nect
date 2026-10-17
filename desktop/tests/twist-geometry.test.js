import test from 'node:test';
import assert from 'node:assert/strict';
import { twistGeometry } from '../src/twist-geometry.js';

const point = (id, position, handleA = [0, 0], handleB = [0, 0], extra = {}) => ({
  id,
  position,
  handleA,
  handleB,
  handleType: 'smooth',
  ...extra,
});

const path = (contours, extra = {}) => ({
  type: 'Path',
  contours,
  metadata: { owner: 'test' },
  ...extra,
});

const close = (actual, expected, tolerance = 1e-8) =>
  assert.ok(Math.abs(actual - expected) <= tolerance, `${actual} ≉ ${expected}`);

function absHandle(point, key) {
  return point.position.map((value, axis) => value + point[key][axis]);
}

function distance(a, b) {
  return Math.hypot(a[0] - b[0], a[1] - b[1]);
}

test('Twist Points fixes the center and turns positive clockwise / negative counterclockwise', () => {
  const source = path([
    {
      id: 'box',
      closed: true,
      points: [
        point('center', [0, 0]),
        point('right', [1, 0]),
        point('near', [0.25, 0]),
        point('left', [-1, 0]),
        point('top', [0, -1]),
        point('bottom', [0, 1]),
      ],
    },
  ]);
  const positive = twistGeometry(source, { angle: 90 }).contours[0].points;
  const negative = twistGeometry(source, { angle: -90 }).contours[0].points;
  const byId = (points, id) =>
    points[['center', 'right', 'near', 'left', 'top', 'bottom'].indexOf(id)];

  assert.deepEqual(byId(positive, 'center').position, [0, 0]);
  assert.ok(byId(positive, 'right').position[1] > 0, 'positive is clockwise with screen Y down');
  assert.ok(byId(negative, 'right').position[1] < 0, 'negative turns counterclockwise');
  close(distance(byId(positive, 'right').position, [0, 0]), 1);
  close(distance(byId(negative, 'right').position, [0, 0]), 1);
  const rightTurn = Math.atan2(
    byId(positive, 'right').position[1],
    byId(positive, 'right').position[0],
  );
  const nearTurn = Math.atan2(
    byId(positive, 'near').position[1],
    byId(positive, 'near').position[0],
  );
  assert.ok(nearTurn > rightTurn, 'closer points receive more angular influence');
});

test('Twist Points preserves anchor and absolute-control radial distances', () => {
  const source = path([
    {
      id: 'curve',
      closed: false,
      group: 'shared-group',
      points: [
        point('a', [-4, -3], [-1, 1], [3, 2]),
        point('b', [5, 1], [-2, 4], [1, -2]),
        point('c', [1, 6], [2, -1], [-4, -3]),
      ],
    },
  ]);
  // Shared bounds include all anchors and absolute control endpoints.
  const endpoints = source.contours[0].points.flatMap((item) => [
    item.position,
    absHandle(item, 'handleA'),
    absHandle(item, 'handleB'),
  ]);
  const center = [
    (Math.min(...endpoints.map((p) => p[0])) + Math.max(...endpoints.map((p) => p[0]))) / 2,
    (Math.min(...endpoints.map((p) => p[1])) + Math.max(...endpoints.map((p) => p[1]))) / 2,
  ];
  const result = twistGeometry(source, { angle: 127 }).contours[0];
  for (const [index, original] of source.contours[0].points.entries()) {
    const changed = result.points[index];
    for (const key of ['position', 'handleA', 'handleB']) {
      const originalAbsolute = key === 'position' ? original.position : absHandle(original, key);
      const changedAbsolute = key === 'position' ? changed.position : absHandle(changed, key);
      close(distance(originalAbsolute, center), distance(changedAbsolute, center));
    }
  }
  assert.equal(result.closed, false);
  assert.equal(result.group, 'shared-group');
});

test('Twist Points uses shared anchor and absolute-control bounds independent of contour order', () => {
  const isolated = {
    id: 'near-contour',
    closed: false,
    role: 'near',
    points: [point('near', [9, 5], [0, 0], [0, 0], { label: 'same-anchor' })],
  };
  const bounds = {
    id: 'bounds-contour',
    closed: true,
    role: 'bounds',
    points: [point('lo', [0, 0]), point('hi', [10, 10], [0, 0], [10, 0])],
  };
  const forward = twistGeometry(path([isolated, bounds]), { angle: 90 });
  const reversed = twistGeometry(path([bounds, isolated]), { angle: 90 });
  const findNear = (geometry) =>
    geometry.contours
      .flatMap((contour) => contour.points)
      .find((item) => item.label === 'same-anchor');
  const a = findNear(forward);
  const b = findNear(reversed);
  assert.notDeepEqual(
    a.position,
    isolated.points[0].position,
    'other contours define the radial frame',
  );
  assert.deepEqual(
    a.position,
    b.position,
    'reordering contours does not alter the shared deformation',
  );
  assert.deepEqual(
    forward.contours.map((contour) => contour.role),
    ['near', 'bounds'],
    'contour ordering and metadata are preserved',
  );
});

test('rectangle corners exactly at R stay unchanged', () => {
  const source = path([
    {
      id: 'rectangle',
      closed: true,
      points: [
        point('tl', [0, 0]),
        point('tr', [100, 0]),
        point('br', [100, 100]),
        point('bl', [0, 100]),
      ],
    },
  ]);
  const result = twistGeometry(source, { angle: 360 });
  assert.deepEqual(
    result.contours[0].points.map((item) => item.position),
    source.contours[0].points.map((item) => item.position),
  );
});

test('Twist Points remaps absolute handles and reconstructs relative vectors', () => {
  const source = path([
    {
      id: 'curve',
      closed: false,
      points: [
        point('start', [0, 0], [0, 0], [100, 50]),
        point('end', [100, 0]),
        point('bottom-left', [0, 100]),
        point('bottom-right', [100, 100]),
      ],
    },
  ]);
  const result = twistGeometry(source, { angle: 90 }).contours[0].points;
  const originalEnd = absHandle(source.contours[0].points[0], 'handleB');
  const resultEnd = absHandle(result[0], 'handleB');
  assert.notDeepEqual(resultEnd, originalEnd);
  assert.notDeepEqual(result[0].handleB, source.contours[0].points[0].handleB);
  close(distance(resultEnd, [50, 50]), distance(originalEnd, [50, 50]));
  assert.ok(
    result.every((item) =>
      [...item.position, ...item.handleA, ...item.handleB].every(Number.isFinite),
    ),
  );
});

test('empty, zero-angle, and zero-radius inputs independently clone', () => {
  const ordinary = path([
    {
      id: 'line',
      closed: false,
      nested: { label: 'ordinary' },
      points: [point('a', [0, 0]), point('b', [10, 10])],
    },
  ]);
  const empty = path([]);
  const degenerate = path([
    { id: 'dot', closed: false, nested: { label: 'keep' }, points: [point('p', [4, 7])] },
  ]);
  for (const [source, angle] of [
    [ordinary, 0],
    [empty, 30],
    [degenerate, 60],
  ]) {
    const result = twistGeometry(source, { angle });
    assert.notEqual(result, source);
    assert.notEqual(result.contours, source.contours);
    if (source.contours.length) {
      assert.notEqual(result.contours[0], source.contours[0]);
      assert.notEqual(result.contours[0].points[0].position, source.contours[0].points[0].position);
      assert.notEqual(result.contours[0].nested, source.contours[0].nested);
      assert.deepEqual(
        result.contours[0].points[0].position,
        source.contours[0].points[0].position,
      );
    }
  }
});

test('Twist Points is deterministic, preserves metadata, and does not mutate input', () => {
  const source = path(
    [
      {
        id: 'curve',
        closed: true,
        group: 'g',
        custom: { tag: 'retained', values: [1, 2] },
        points: [point('a', [-1, -1]), point('b', [1, 1], [0.25, 0.5])],
      },
    ],
    { extra: { source: 'fixture' } },
  );
  const before = JSON.stringify(source);
  const options = { angle: -245, prefix: 'stable' };
  const first = twistGeometry(source, options);
  const second = twistGeometry(source, options);
  assert.deepEqual(first, second);
  assert.equal(first.metadata.owner, 'test');
  assert.deepEqual(first.extra, { source: 'fixture' });
  assert.deepEqual(first.contours[0].custom, { tag: 'retained', values: [1, 2] });
  assert.equal(first.contours[0].id, 'stable:c0');
  assert.equal(first.contours[0].points[0].id, 'stable:c0:p0');
  assert.equal(JSON.stringify(source), before);
  first.metadata.owner = 'changed';
  first.contours[0].custom.values.push(3);
  assert.equal(source.metadata.owner, 'test');
  assert.deepEqual(source.contours[0].custom.values, [1, 2]);
});

test('Twist Points rejects bad ranges, source vectors, endpoint overflow, output overflow, and budgets', () => {
  const simple = path([
    { id: 'line', closed: false, points: [point('a', [0, 0]), point('b', [100, 10])] },
  ]);
  for (const angle of [NaN, Infinity, -361, 361])
    assert.throws(() => twistGeometry(simple, { angle }), /finite|between -360 and 360/);
  assert.throws(() => twistGeometry(simple, { angle: 1, prefix: '' }), /prefix/);
  assert.throws(
    () =>
      twistGeometry(path([{ id: 'bad', closed: false, points: [point('p', [Infinity, 0])] }]), {
        angle: 1,
      }),
    /finite positions and handles/,
  );
  assert.throws(
    () =>
      twistGeometry(
        path([{ id: 'bad', closed: false, points: [point('p', [1e308, 0], [1e308, 0])] }]),
        { angle: 1 },
      ),
    /finite-coordinate arithmetic/,
  );

  const huge = Number.MAX_VALUE;
  const overflowingOutput = path([
    {
      id: 'huge',
      closed: false,
      points: [
        point('lo', [huge / 4, -huge]),
        point('hi', [huge * 0.75, huge]),
        point('interior', [huge / 2, huge * 0.75]),
      ],
    },
  ]);
  assert.throws(
    () => twistGeometry(overflowingOutput, { angle: -360 }),
    /finite-coordinate arithmetic/,
  );

  assert.throws(
    () =>
      twistGeometry(
        path(Array.from({ length: 10001 }, (_, i) => ({ id: `c${i}`, closed: false, points: [] }))),
        { angle: 1 },
      ),
    /geometry budget/,
  );
  assert.throws(
    () =>
      twistGeometry(
        path([
          {
            id: 'many',
            closed: false,
            points: Array.from({ length: 100001 }, (_, i) => point(`p${i}`, [0, 0])),
          },
        ]),
        { angle: 1 },
      ),
    /geometry budget/,
  );
});
