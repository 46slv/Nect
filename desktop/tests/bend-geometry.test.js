import test from 'node:test';
import assert from 'node:assert/strict';
import { bendGeometry } from '../src/bend-geometry.js';

const point = (id, position, handleA = [0, 0], handleB = [0, 0]) => ({
  id,
  position,
  handleA,
  handleB,
  handleType: 'smooth',
});

function path(contours) {
  return { type: 'Path', contours, metadata: { owner: 'test' } };
}

const close = (actual, expected, tolerance = 1e-9) =>
  assert.ok(Math.abs(actual - expected) <= tolerance, `${actual} ≉ ${expected}`);

test('Bend Points bends a horizontal strip in both directions', () => {
  const source = path([
    {
      id: 'line',
      closed: false,
      role: 'source',
      points: [point('left', [0, 0]), point('right', [100, 0])],
    },
  ]);
  const positive = bendGeometry(source, { angle: 90 });
  const negative = bendGeometry(source, { angle: -90 });
  const bend = (50 * (1 - Math.cos(Math.PI / 4))) / (Math.PI / 4);
  const axial = (50 * Math.sin(Math.PI / 4)) / (Math.PI / 4);

  close(positive.contours[0].points[0].position[0], 50 - axial);
  close(positive.contours[0].points[1].position[0], 50 + axial);
  close(positive.contours[0].points[0].position[1], bend);
  close(positive.contours[0].points[1].position[1], bend);
  close(negative.contours[0].points[0].position[1], -bend);
  close(negative.contours[0].points[1].position[1], -bend);
});

test('Bend Points swaps the selected axis for vertical operation', () => {
  const source = path([
    { id: 'line', closed: false, points: [point('top', [0, 0]), point('bottom', [0, 100])] },
  ]);
  const vertical = bendGeometry(source, { angle: 90, axis: 'vertical' }).contours[0].points;
  const reversed = bendGeometry(source, { angle: -90, axis: 'vertical' }).contours[0].points;
  const bend = (50 * (1 - Math.cos(Math.PI / 4))) / (Math.PI / 4);
  const axial = (50 * Math.sin(Math.PI / 4)) / (Math.PI / 4);
  close(vertical[0].position[1], 50 - axial);
  close(vertical[1].position[1], 50 + axial);
  close(vertical[0].position[0], bend);
  close(vertical[1].position[0], bend);
  close(reversed[0].position[0], -bend);
  close(reversed[1].position[0], -bend);
});

test('Bend Points uses one shared anchor and absolute-handle bounds', () => {
  const source = path([
    { id: 'a', closed: false, points: [point('a0', [25, 20])] },
    { id: 'b', closed: true, points: [point('b0', [25, 20], [-25, -20], [75, 20])] },
  ]);
  const result = bendGeometry(source, { angle: 45 });
  assert.deepEqual(result.contours[0].points[0].position, result.contours[1].points[0].position);
  assert.notDeepEqual(result.contours[0].points[0].position, source.contours[0].points[0].position);
  assert.deepEqual(
    result.contours.map((contour) => contour.closed),
    [false, true],
  );
  assert.deepEqual(
    result.contours.map((contour) => contour.id),
    ['bend:c0', 'bend:c1'],
  );
});

test('Bend Points remaps absolute handles and reconstructs relative handles', () => {
  const source = path([
    {
      id: 'curve',
      closed: false,
      points: [point('start', [0, 0], [0, 0], [100, 0]), point('end', [100, 0])],
    },
  ]);
  const result = bendGeometry(source, { angle: 60 });
  const [start, end] = result.contours[0].points;
  assert.deepEqual(
    start.position.map((value, i) => value + start.handleB[i]),
    end.position,
  );
  assert.deepEqual(start.handleA, [0, 0]);
});

test('zero angle, empty geometry, and zero axial span independently clone', () => {
  const source = path([{ id: 'flat', closed: false, points: [point('p', [4, 7])] }]);
  for (const [input, options] of [
    [source, { angle: 0 }],
    [path([]), { angle: 40 }],
    [source, { angle: 40, axis: 'vertical' }],
  ]) {
    const result = bendGeometry(input, options);
    assert.notEqual(result, input);
    assert.notEqual(result.contours, input.contours);
    if (input.contours.length) {
      assert.notEqual(result.contours[0], input.contours[0]);
      assert.notEqual(result.contours[0].points[0].position, input.contours[0].points[0].position);
      assert.deepEqual(result.contours[0].points[0].position, input.contours[0].points[0].position);
    }
  }
});

test('small angles remain continuous with the straight input', () => {
  const source = path([
    { id: 'line', closed: false, points: [point('a', [0, 0]), point('b', [100, 20])] },
  ]);
  const bent = bendGeometry(source, { angle: 1e-7 }).contours[0].points;
  for (const [index, item] of bent.entries())
    for (let axis = 0; axis < 2; axis++)
      assert.ok(
        Math.abs(item.position[axis] - source.contours[0].points[index].position[axis]) < 1e-6,
      );
});

test('Bend Points preserves metadata, is deterministic, and leaves source untouched', () => {
  const source = path([
    { id: 'curve', closed: true, group: 'g', points: [point('a', [0, 0]), point('b', [100, 0])] },
  ]);
  const before = JSON.stringify(source);
  const first = bendGeometry(source, { angle: 25, prefix: 'stable' });
  const second = bendGeometry(source, { angle: 25, prefix: 'stable' });
  assert.deepEqual(first, second);
  assert.equal(first.metadata.owner, 'test');
  assert.equal(first.contours[0].group, 'g');
  assert.equal(first.contours[0].closed, true);
  assert.equal(first.contours[0].points[0].id, 'stable:c0:p0');
  assert.equal(JSON.stringify(source), before);
});

test('Bend Points rejects invalid angle, axis, vectors, and non-finite handle bounds', () => {
  const simple = path([
    { id: 'line', closed: false, points: [point('a', [0, 0]), point('b', [100, 0])] },
  ]);
  assert.throws(() => bendGeometry(simple, { angle: Infinity }), /finite/);
  assert.throws(() => bendGeometry(simple, { angle: 181 }), /-180 and 180/);
  assert.throws(() => bendGeometry(simple, { axis: 'depth' }), /axis/);
  assert.throws(
    () =>
      bendGeometry(path([{ id: 'bad', closed: false, points: [point('p', [Infinity, 0])] }]), {
        angle: 1,
      }),
    /finite positions and handles/,
  );
  assert.throws(
    () =>
      bendGeometry(
        path([{ id: 'bad', closed: false, points: [point('p', [1e308, 0], [1e308, 0])] }]),
        { angle: 1 },
      ),
    /finite-coordinate arithmetic/,
  );
});

test('Bend Points rejects folds for either angle sign and finite arithmetic overflow', () => {
  const folded = path([
    { id: 'wide', closed: true, points: [point('a', [0, -40]), point('b', [100, 40])] },
  ]);
  assert.throws(() => bendGeometry(folded, { angle: 180 }), /fold/);
  assert.throws(() => bendGeometry(folded, { angle: -180 }), /fold/);

  const overflowingSpan = path([
    {
      id: 'huge',
      closed: false,
      points: [point('a', [-Number.MAX_VALUE, 0]), point('b', [Number.MAX_VALUE, 0])],
    },
  ]);
  assert.throws(() => bendGeometry(overflowingSpan, { angle: 1 }), /finite-coordinate arithmetic/);

  const overflowingOutput = path([
    {
      id: 'near-limit',
      closed: false,
      points: [point('a', [0, 1.79e308]), point('b', [1e308, 1.79e308])],
    },
  ]);
  assert.throws(
    () => bendGeometry(overflowingOutput, { angle: 90 }),
    /finite-coordinate arithmetic/,
  );
});

test('Bend Points enforces contour and point budgets', () => {
  assert.throws(
    () =>
      bendGeometry(
        path(Array.from({ length: 10001 }, (_, i) => ({ id: `c${i}`, closed: false, points: [] }))),
        { angle: 1 },
      ),
    /geometry budget/,
  );
  const tooManyPoints = path([
    {
      id: 'many',
      closed: false,
      points: Array.from({ length: 100001 }, (_, i) => point(`p${i}`, [0, 0])),
    },
  ]);
  assert.throws(() => bendGeometry(tooManyPoints, { angle: 1 }), /geometry budget/);
});
