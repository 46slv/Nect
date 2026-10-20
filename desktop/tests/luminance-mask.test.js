import test from 'node:test';
import assert from 'node:assert/strict';
import {
  clipLuminanceMarks,
  LuminanceMaskError,
  LUMINANCE_MASK_LIMITS,
  prepareLuminanceMask,
} from '../src/luminance-mask.js';

const point = (position, id = 'p', { handleA = [0, 0], handleB = [0, 0] } = {}) => ({
  id,
  position,
  handleA,
  handleB,
});
const contour = (id, positions, { reverse = false, handles = {} } = {}) => {
  const ring = reverse ? [...positions].reverse() : positions;
  return {
    id,
    closed: true,
    points: ring.map((position, index) => point(position, `${id}:p${index}`, handles[index] ?? {})),
  };
};
const rectangle = (id, x0, y0, x1, y1, options) =>
  contour(
    id,
    [
      [x0, y0],
      [x1, y0],
      [x1, y1],
      [x0, y1],
    ],
    options,
  );
const path = (...contours) => ({ contours });
const box = (x0, y0, x1, y1) => [
  [x0, y0],
  [x1, y0],
  [x1, y1],
  [x0, y1],
];
const signedArea = (ring) =>
  ring.reduce((sum, a, index) => {
    const b = ring[(index + 1) % ring.length];
    return sum + a.position[0] * b.position[1] - b.position[0] * a.position[1];
  }, 0) / 2;
const errorCode = (code, fn) =>
  assert.throws(fn, (error) => error instanceof LuminanceMaskError && error.code === code);

test('prepares closed ungrouped polygons with bounded bounds and stable winding', () => {
  const mask = prepareLuminanceMask(path(rectangle('outer', -10, -5, 30, 25)));
  assert.deepEqual(mask.polygons, [box(-10, -5, 30, 25)]);
  assert.equal(mask.flattenedVertices, 4);
  assert.equal(mask.subdivisionNodes, 4);
  assert.ok(mask.bounds.minX < -10);
  assert.ok(mask.bounds.minY < -5);
  assert.ok(mask.bounds.maxX > 30);
  assert.ok(mask.bounds.maxY > 25);
  assert.deepEqual(mask.origin, { x: -10, y: -5 });
});

test('NonZero intersection preserves holes, winding, and excludes leaked marks', () => {
  const withHole = prepareLuminanceMask(
    path(rectangle('outer', 0, 0, 20, 20), rectangle('hole', 5, 5, 15, 15, { reverse: true })),
  );
  assert.deepEqual(clipLuminanceMarks(withHole, [box(7, 7, 13, 13)], 'hole-test'), {
    contours: [],
  });

  const clipped = clipLuminanceMarks(withHole, [box(-10, -10, 30, 30)], 'ring-test');
  assert.equal(clipped.contours.length, 2);
  const areas = clipped.contours.map((ring) => signedArea(ring.points));
  assert.ok(areas.some((area) => area > 0));
  assert.ok(areas.some((area) => area < 0));
  for (const contour of clipped.contours)
    for (const point of contour.points) {
      assert.ok(point.position[0] >= -1e-4 && point.position[0] <= 20 + 1e-4);
      assert.ok(point.position[1] >= -1e-4 && point.position[1] <= 20 + 1e-4);
      assert.deepEqual(point.handleA, [0, 0]);
      assert.deepEqual(point.handleB, [0, 0]);
    }

  const sameWinding = prepareLuminanceMask(
    path(rectangle('outer', 0, 0, 20, 20), rectangle('inner', 5, 5, 15, 15)),
  );
  const filled = clipLuminanceMarks(sameWinding, [box(7, 7, 13, 13)], 'filled-test');
  assert.equal(filled.contours.length, 1);
});

test('clips against negative-coordinate source geometry and returns deterministic Path IDs', () => {
  const mask = prepareLuminanceMask(path(rectangle('negative', -30, -25, -10, -5)));
  const mark = box(-40, -40, 0, 0);
  const first = clipLuminanceMarks(mask, [mark], 'deterministic');
  const second = clipLuminanceMarks(mask, [mark], 'deterministic');
  assert.deepEqual(first, second);
  assert.equal(first.contours.length, 1);
  assert.deepEqual(first.contours[0].id, 'deterministic:c0');
  assert.deepEqual(
    first.contours[0].points.map(({ id }) => id),
    first.contours[0].points.map((_, index) => `deterministic:c0:p${index}`),
  );
  assert.ok(
    first.contours[0].points.every(({ position }) =>
      position.every((coordinate) => coordinate < 0),
    ),
  );
  assert.ok(
    first.contours[0].points.every(
      ({ handleA, handleB }) =>
        handleA[0] === 0 && handleA[1] === 0 && handleB[0] === 0 && handleB[1] === 0,
    ),
  );
});

test('flattens source cubic curves at the shared bounded 0.1 px tolerance', () => {
  const curved = contour(
    'curve',
    [
      [-10, 0],
      [10, 0],
      [0, -10],
    ],
    {
      handles: {
        0: { handleB: [0, -10] },
        1: { handleA: [0, -10] },
      },
    },
  );
  const mask = prepareLuminanceMask(path(curved));
  assert.ok(mask.flattenedVertices > 3);
  assert.ok(mask.flattenedVertices <= LUMINANCE_MASK_LIMITS.flattenedVertices);
  assert.ok(mask.subdivisionNodes > mask.flattenedVertices);
  assert.ok(mask.bounds.minY < -7.4, `curve bound did not contain its bulge: ${mask.bounds.minY}`);
  assert.ok(mask.bounds.maxY > 0);
});

test('rejects open and grouped paths explicitly, malformed inputs, and integer-grid collapse', () => {
  const open = rectangle('open', 0, 0, 10, 10);
  open.closed = false;
  errorCode('LUMINANCE_MASK_OPEN_PATH', () => prepareLuminanceMask(path(open)));
  const grouped = rectangle('grouped', 0, 0, 10, 10);
  grouped.group = 'g1';
  errorCode('LUMINANCE_MASK_UNSUPPORTED', () => prepareLuminanceMask(path(grouped)));
  errorCode('LUMINANCE_MASK_GEOMETRY', () => prepareLuminanceMask(null));
  errorCode('LUMINANCE_MASK_GEOMETRY', () => prepareLuminanceMask({ contours: [] }));
  const invalid = rectangle('invalid', 0, 0, 10, 10);
  invalid.points[0].position[0] = Infinity;
  errorCode('LUMINANCE_MASK_GEOMETRY', () => prepareLuminanceMask(path(invalid)));
  const collapsed = rectangle('collapsed', 0, 0, 0.00004, 1);
  errorCode('LUMINANCE_MASK_PRECISION', () => prepareLuminanceMask(path(collapsed)));
  const mask = prepareLuminanceMask(path(rectangle('source', 0, 0, 10, 10)));
  errorCode('LUMINANCE_MASK_GEOMETRY', () =>
    clipLuminanceMarks(
      mask,
      [
        [
          [0, 0],
          [1, 0],
        ],
      ],
      'bad',
    ),
  );
  errorCode('LUMINANCE_MASK_OPTIONS', () => clipLuminanceMarks(mask, [], ''));
});

test('rejects authored, generated, and intersection-budget overflow before running the kernel', () => {
  const tooManyContours = path(
    ...Array.from({ length: LUMINANCE_MASK_LIMITS.contours + 1 }, (_, index) =>
      rectangle(`c${index}`, index * 4, 0, index * 4 + 2, 2),
    ),
  );
  errorCode('LUMINANCE_MASK_LIMIT', () => prepareLuminanceMask(tooManyContours));

  const tooManyPoints = contour(
    'large',
    Array.from({ length: LUMINANCE_MASK_LIMITS.sourcePoints + 1 }, (_, index) => [
      index,
      index % 2,
    ]),
  );
  errorCode('LUMINANCE_MASK_LIMIT', () => prepareLuminanceMask(path(tooManyPoints)));

  const mask = {
    origin: { x: 0, y: 0 },
    polygons: Array.from({ length: 8 }, (_, index) =>
      Array.from({ length: 64 }, (_, pointIndex) => {
        const angle = (pointIndex / 64) * Math.PI * 2;
        return [Math.cos(angle) * 10 + index * 30, Math.sin(angle) * 10];
      }),
    ),
  };
  assert.equal(
    mask.polygons.reduce((sum, ring) => sum + ring.length, 0),
    512, // This fixture is chosen to cross the unchanged 2000000 pair-work cap.
  );
  const tooManyGenerated = Array.from({ length: 25001 }, (_, index) =>
    box(index * 3, 1000, index * 3 + 1, 1001),
  );
  errorCode('LUMINANCE_MASK_LIMIT', () => clipLuminanceMarks(mask, tooManyGenerated, 'many'));
  const tooMuchPairWork = Array.from({ length: 1001 }, (_, index) =>
    box(index * 3, 1000, index * 3 + 1, 1001),
  );
  errorCode('LUMINANCE_MASK_LIMIT', () => clipLuminanceMarks(mask, tooMuchPairWork, 'work'));
});

test('enforces the flattened-vertex cap and rejects oversized kernel output without returning partial geometry', () => {
  const count = 1024;
  const radius = 10000;
  const tangentHandle = (angle) => {
    const scale = radius; // Deliberately looping controls exceed the larger flattening budget.
    const clean = (value) => (Math.abs(value) < 1e-9 ? 0 : value);
    return [clean(-Math.sin(angle) * scale), clean(Math.cos(angle) * scale)];
  };
  const denseCurvedRing = contour(
    'dense-curves',
    Array.from({ length: count }, (_, index) => {
      const angle = (index / count) * Math.PI * 2;
      return [radius * Math.cos(angle), radius * Math.sin(angle)];
    }),
    {
      handles: Object.fromEntries(
        Array.from({ length: count }, (_, index) => {
          const angle = (index / count) * Math.PI * 2;
          const outgoing = tangentHandle(angle);
          return [index, { handleA: outgoing.map((component) => -component), handleB: outgoing }];
        }),
      ),
    },
  );
  errorCode('LUMINANCE_MASK_LIMIT', () => prepareLuminanceMask(path(denseCurvedRing)));

  const wideMask = prepareLuminanceMask(path(rectangle('wide', 0, -2, 30010, 2)));
  const tooManyOutputContours = Array.from({ length: 10001 }, (_, index) =>
    box(index * 3, 0, index * 3 + 1, 1),
  );
  errorCode('LUMINANCE_MASK_LIMIT', () =>
    clipLuminanceMarks(wideMask, tooManyOutputContours, 'output-limit'),
  );
});

test('rejects unsafe local integer coordinates and non-finite generated points', () => {
  const tooWide = rectangle('wide', 0, 0, 200000, 1);
  errorCode('LUMINANCE_MASK_RANGE', () => prepareLuminanceMask(path(tooWide)));
  const mask = prepareLuminanceMask(path(rectangle('source', 0, 0, 10, 10)));
  errorCode('LUMINANCE_MASK_GEOMETRY', () =>
    clipLuminanceMarks(
      mask,
      [
        [
          [0, 0],
          [Infinity, 0],
          [0, 1],
        ],
      ],
      'bad',
    ),
  );
});

test('exact zero-length font segments preserve fill and holes without changing authored data', () => {
  const outer = rectangle('outer', 0, 0, 100, 100),
    hole = rectangle('hole', 20, 20, 80, 80, { reverse: true });
  const plain = path(outer, hole),
    repeated = structuredClone(plain);
  for (const c of repeated.contours) {
    c.points.splice(1, 0, structuredClone(c.points[0]));
    c.points.push(structuredClone(c.points[0]));
  }
  const before = JSON.stringify(repeated),
    prepared = prepareLuminanceMask(repeated);
  assert.deepEqual(prepared.polygons, prepareLuminanceMask(plain).polygons);
  assert.equal(JSON.stringify(repeated), before);
  const marks = [box(-1, -1, 101, 101)];
  assert.deepEqual(
    clipLuminanceMarks(prepared, marks, 'same'),
    clipLuminanceMarks(prepareLuminanceMask(plain), marks, 'same'),
  );
});
