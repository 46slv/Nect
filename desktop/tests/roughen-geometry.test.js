import test from 'node:test';
import assert from 'node:assert/strict';
import { roughenGeometry } from '../src/roughen-geometry.js';

const point = (id, position, handleA = [0, 0], handleB = [0, 0]) => ({
  id,
  position,
  handleA,
  handleB,
});
const sourcePath = () => ({
  name: 'source path',
  contours: [
    {
      id: 'outline',
      closed: false,
      group: 'shape-a',
      points: [
        point('start', [0, 0], [-2, 1], [3, 4]),
        point('middle', [20, 10], [-1, 5], [6, -3]),
        point('end', [40, 0], [2, -1], [4, 2]),
      ],
    },
    {
      id: 'hole',
      closed: true,
      group: 'shape-a',
      points: [point('hole-a', [5, 5]), point('hole-b', [10, 5]), point('hole-c', [8, 9])],
    },
  ],
});

test('is deterministic by stable source IDs and changes when the seed changes', () => {
  const source = sourcePath();
  const options = { amount: 12, seed: 42, keepEndpoints: false, prefix: 'noise' };
  const first = roughenGeometry(source, options);
  assert.deepEqual(roughenGeometry(source, options), first);

  const changed = roughenGeometry(source, { ...options, seed: 43 });
  assert.notDeepEqual(changed.contours[0].points[1].position, first.contours[0].points[1].position);

  const reordered = {
    contours: source.contours.map((contour) => ({
      ...contour,
      points: [...contour.points].reverse(),
    })),
  };
  const reorderedResult = roughenGeometry(reordered, options);
  source.contours.forEach((contour, contourIndex) =>
    contour.points.forEach((sourcePoint, pointIndex) => {
      const originalPoint = first.contours[contourIndex].points[pointIndex];
      const reorderedSourceContour = reordered.contours[contourIndex];
      const reorderedIndex = reorderedSourceContour.points.findIndex(
        (candidate) => candidate.id === sourcePoint.id,
      );
      const reorderedPoint = reorderedResult.contours[contourIndex].points[reorderedIndex];
      assert.deepEqual(
        originalPoint.position.map((value, axis) => value - sourcePoint.position[axis]),
        reorderedPoint.position.map((value, axis) => value - sourcePoint.position[axis]),
      );
    }),
  );
});

test('offsets are per-axis bounded by amount and scale linearly with amount', () => {
  const source = sourcePath();
  const small = roughenGeometry(source, {
    amount: 4,
    seed: 77,
    keepEndpoints: false,
    prefix: 'small',
  });
  const large = roughenGeometry(source, {
    amount: 20,
    seed: 77,
    keepEndpoints: false,
    prefix: 'large',
  });
  source.contours.forEach((contour, contourIndex) =>
    contour.points.forEach((sourcePoint, pointIndex) => {
      const smallPoint = small.contours[contourIndex].points[pointIndex];
      const largePoint = large.contours[contourIndex].points[pointIndex];
      sourcePoint.position.forEach((coordinate, axis) => {
        const delta = smallPoint.position[axis] - coordinate;
        assert.ok(Math.abs(delta) <= 4);
        assert.ok(Math.abs(largePoint.position[axis] - coordinate) <= 20);
        assert.ok(Math.abs(largePoint.position[axis] - coordinate - delta * 5) < 1e-12);
      });
    }),
  );
});

test('pins only first and last points of open contours by default', () => {
  const source = sourcePath();
  const result = roughenGeometry(source, { amount: 50, seed: 4 });
  const open = result.contours[0];
  assert.deepEqual(open.points[0].position, source.contours[0].points[0].position);
  assert.deepEqual(open.points.at(-1).position, source.contours[0].points.at(-1).position);
  assert.notDeepEqual(open.points[1].position, source.contours[0].points[1].position);

  const unpinned = roughenGeometry(source, {
    amount: 50,
    seed: 4,
    keepEndpoints: false,
  });
  assert.notDeepEqual(
    unpinned.contours[0].points[0].position,
    source.contours[0].points[0].position,
  );
  assert.notDeepEqual(
    unpinned.contours[0].points.at(-1).position,
    source.contours[0].points.at(-1).position,
  );

  const singlePoint = {
    contours: [{ id: 'single', closed: false, points: [point('only', [1, 2])] }],
  };
  assert.deepEqual(
    roughenGeometry(singlePoint, { amount: 5, seed: 1 }).contours[0].points[0].position,
    [1, 2],
  );
});

test('moves closed contours, preserves groups and relative handles, and gives unique namespaced IDs', () => {
  const source = sourcePath();
  const result = roughenGeometry(source, {
    amount: 25,
    seed: 5,
    keepEndpoints: true,
    prefix: 'rough',
  });
  assert.equal(result.contours[0].closed, false);
  assert.equal(result.contours[1].closed, true);
  assert.deepEqual(
    result.contours.map((contour) => contour.group),
    ['shape-a', 'shape-a'],
  );
  assert.notDeepEqual(result.contours[1].points[0].position, source.contours[1].points[0].position);

  const pointIds = result.contours.flatMap((contour) => contour.points.map((item) => item.id));
  assert.equal(new Set(result.contours.map((contour) => contour.id)).size, result.contours.length);
  assert.equal(new Set(pointIds).size, pointIds.length);
  assert.ok(result.contours.every((contour, index) => contour.id === `rough:c${index}`));
  assert.ok(pointIds.every((id) => id.startsWith('rough:c')));

  source.contours.forEach((contour, contourIndex) =>
    contour.points.forEach((sourcePoint, pointIndex) => {
      const resultPoint = result.contours[contourIndex].points[pointIndex];
      assert.deepEqual(resultPoint.handleA, sourcePoint.handleA);
      assert.deepEqual(resultPoint.handleB, sourcePoint.handleB);
      const delta = resultPoint.position.map((value, axis) => value - sourcePoint.position[axis]);
      assert.deepEqual(
        resultPoint.position.map((value, axis) => value + resultPoint.handleA[axis]),
        sourcePoint.position.map((value, axis) => value + sourcePoint.handleA[axis] + delta[axis]),
      );
      assert.deepEqual(
        resultPoint.position.map((value, axis) => value + resultPoint.handleB[axis]),
        sourcePoint.position.map((value, axis) => value + sourcePoint.handleB[axis] + delta[axis]),
      );
    }),
  );
});

test('does not mutate inputs and amount zero returns an independent geometry-equivalent copy', () => {
  const source = sourcePath();
  const before = structuredClone(source);
  const result = roughenGeometry(source, { amount: 0, seed: 7 });
  assert.deepEqual(source, before);
  assert.deepEqual(result, source);
  assert.notEqual(result, source);
  assert.notEqual(result.contours, source.contours);
  assert.notEqual(result.contours[0], source.contours[0]);
  assert.notEqual(result.contours[0].points[0].position, source.contours[0].points[0].position);

  result.contours[0].points[0].position[0] = 900;
  assert.equal(source.contours[0].points[0].position[0], 0);
  roughenGeometry(source, { amount: 10, seed: 99 });
  assert.deepEqual(source, before);
});

test('rejects malformed options, invalid geometry, and excessive geometry budgets', () => {
  const source = sourcePath();
  for (const amount of [-1, Infinity, NaN, 100001])
    assert.throws(() => roughenGeometry(source, { amount, seed: 0 }), /amount/i);
  for (const seed of [-1, 1.5, Infinity, 2147483648])
    assert.throws(() => roughenGeometry(source, { amount: 1, seed }), /seed/i);
  assert.throws(
    () => roughenGeometry(source, { amount: 1, seed: 0, keepEndpoints: 1 }),
    /keepEndpoints/i,
  );
  assert.throws(() => roughenGeometry(source, { amount: 1, seed: 0, prefix: '' }), /prefix/i);
  assert.throws(() => roughenGeometry(source, null), /options/i);
  assert.throws(() => roughenGeometry(null, { amount: 1, seed: 0 }), /Path input/);

  const invalid = sourcePath();
  invalid.contours[0].points[1].handleB[0] = Infinity;
  assert.throws(() => roughenGeometry(invalid, { amount: 1, seed: 0 }), /finite/i);

  assert.throws(
    () =>
      roughenGeometry({ contours: Array(10001).fill(source.contours[0]) }, { amount: 1, seed: 0 }),
    /budget/i,
  );
  const tooManyPoints = {
    contours: [
      {
        id: 'large',
        closed: false,
        points: Array.from({ length: 100001 }, (_, index) => point(`p${index}`, [0, 0])),
      },
    ],
  };
  assert.throws(() => roughenGeometry(tooManyPoints, { amount: 1, seed: 0 }), /budget/i);
});
