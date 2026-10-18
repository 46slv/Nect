import test from 'node:test';
import assert from 'node:assert/strict';
import { simplifyGeometry } from '../src/simplify-geometry.js';

const point = (id, position, handleA = [0, 0], handleB = [0, 0], extra = {}) => ({
  id,
  position,
  handleA,
  handleB,
  ...extra,
});

function pathFromContours(contours, extra = {}) {
  return { name: 'source path', metadata: { source: 'test' }, ...extra, contours };
}

function contour(id, positions, { closed = false, group, handles } = {}) {
  return {
    id,
    closed,
    ...(group === undefined ? {} : { group }),
    points: positions.map((position, index) =>
      point(
        `source-${id}-${index}`,
        position,
        ...(handles?.[index] ?? [
          [0, 0],
          [0, 0],
        ]),
      ),
    ),
  };
}

test('reduces redundant open-line points and preserves exact endpoints', () => {
  const source = pathFromContours([
    contour('line', [
      [0, 0],
      [2, 0],
      [4, 0],
      [6, 0],
      [8, 0],
    ]),
  ]);
  const result = simplifyGeometry(source, { tolerance: 0.01 });
  const output = result.contours[0];

  assert.deepEqual(
    output.points.map((item) => item.position),
    [
      [0, 0],
      [8, 0],
    ],
  );
  assert.equal(output.closed, false);
  assert.ok(output.id.startsWith('simplify:'));
  assert.ok(output.points.every((item) => item.id.startsWith('simplify:')));
  assert.deepEqual(output.points[0].position, source.contours[0].points[0].position);
  assert.deepEqual(output.points.at(-1).position, source.contours[0].points.at(-1).position);
});

test('keeps a kink above tolerance and removes it at a larger tolerance', () => {
  const source = pathFromContours([
    contour('kink', [
      [0, 0],
      [5, 2],
      [10, 0],
    ]),
  ]);

  assert.equal(simplifyGeometry(source, { tolerance: 1 }).contours[0].points.length, 3);
  assert.deepEqual(
    simplifyGeometry(source, { tolerance: 2.01 }).contours[0].points.map((item) => item.position),
    [
      [0, 0],
      [10, 0],
    ],
  );
});

test('simplifies closed square edges while keeping seam, winding, and source order', () => {
  const source = pathFromContours([
    contour(
      'square',
      [
        [0, 0],
        [2, 0],
        [4, 0],
        [4, 2],
        [4, 4],
        [2, 4],
        [0, 4],
        [0, 2],
      ],
      { closed: true, group: 'outer' },
    ),
  ]);
  const result = simplifyGeometry(source, { tolerance: 0.01 });

  assert.deepEqual(
    result.contours[0].points.map((item) => item.position),
    [
      [0, 0],
      [4, 0],
      [4, 4],
      [0, 4],
    ],
  );
  assert.equal(result.contours[0].closed, true);
  assert.equal(result.contours[0].group, 'outer');
  assert.deepEqual(result.contours[0].points[0].position, [0, 0]);

  const reversed = pathFromContours([
    contour(
      'reversed-square',
      [
        [0, 0],
        [0, 2],
        [0, 4],
        [2, 4],
        [4, 4],
        [4, 2],
        [4, 0],
        [2, 0],
      ],
      { closed: true },
    ),
  ]);
  assert.deepEqual(
    simplifyGeometry(reversed, { tolerance: 0.01 }).contours[0].points.map((item) => item.position),
    [
      [0, 0],
      [0, 4],
      [4, 4],
      [4, 0],
    ],
  );
});

test('falls back to the original valid ring when high tolerance would collapse it', () => {
  const source = pathFromContours([
    contour(
      'triangle',
      [
        [0, 0],
        [6, 0],
        [3, 5],
      ],
      { closed: true },
    ),
  ]);
  const result = simplifyGeometry(source, { tolerance: 100000 });

  assert.deepEqual(
    result.contours[0].points.map((item) => item.position),
    source.contours[0].points.map((item) => item.position),
  );
  assert.equal(result.contours[0].closed, true);
});

test('preserves contour ordering, groups, and closure flags', () => {
  const source = pathFromContours([
    contour(
      'a',
      [
        [0, 0],
        [1, 0],
        [2, 0],
      ],
      { group: 'shape-a' },
    ),
    contour(
      'b',
      [
        [10, 0],
        [11, 0],
        [12, 0],
      ],
      { closed: true, group: 'shape-b' },
    ),
    contour('c', [
      [20, 0],
      [21, 1],
      [22, 0],
    ]),
  ]);
  const result = simplifyGeometry(source, { tolerance: 0.01, prefix: 'node-7' });

  assert.deepEqual(
    result.contours.map((item) => item.group),
    ['shape-a', 'shape-b', undefined],
  );
  assert.deepEqual(
    result.contours.map((item) => item.closed),
    [false, true, false],
  );
  assert.deepEqual(
    result.contours.map((item) => item.id),
    ['node-7:c0', 'node-7:c1', 'node-7:c2'],
  );
  assert.ok(result.contours.every((item) => item.points.every((p) => p.id.startsWith('node-7:'))));
});

test('zero tolerance returns a geometry-equivalent independent clone with original IDs and handles', () => {
  const source = pathFromContours([
    contour(
      'curve',
      [
        [0, 0],
        [4, 8],
        [10, 0],
      ],
      {
        closed: false,
        handles: [
          [
            [1, 2],
            [3, 4],
          ],
          [
            [-2, 5],
            [3, -1],
          ],
          [
            [6, 7],
            [8, 9],
          ],
        ],
      },
    ),
  ]);
  const before = structuredClone(source);
  const result = simplifyGeometry(source, { tolerance: 0, prefix: 'unused' });

  assert.deepEqual(result, source);
  assert.notEqual(result, source);
  assert.notEqual(result.contours, source.contours);
  assert.notEqual(result.contours[0].points, source.contours[0].points);
  assert.notEqual(result.contours[0].points[0].position, source.contours[0].points[0].position);
  assert.notEqual(result.contours[0].points[1].handleA, source.contours[0].points[1].handleA);
  result.contours[0].points[0].position[0] = 123;
  assert.deepEqual(source, before);
});

test('is deterministic and never mutates the source', () => {
  const source = pathFromContours([
    contour('zig', [
      [0, 0],
      [2, 2],
      [4, -1],
      [6, 3],
      [8, 0],
    ]),
    contour(
      'loop',
      [
        [0, 0],
        [4, 0],
        [4, 4],
        [0, 4],
      ],
      { closed: true, group: 'loop' },
    ),
  ]);
  const before = structuredClone(source);
  const first = simplifyGeometry(source, { tolerance: 0.25, prefix: 'stable' });
  const second = simplifyGeometry(source, { tolerance: 0.25, prefix: 'stable' });

  assert.deepEqual(first, second);
  assert.deepEqual(source, before);
});

test('rejects invalid options, curved handles, invalid coordinates, and geometry budgets', () => {
  const source = pathFromContours([
    contour('line', [
      [0, 0],
      [1, 2],
      [3, 4],
    ]),
  ]);
  for (const tolerance of [-1, Infinity, NaN, 100001, undefined])
    assert.throws(() => simplifyGeometry(source, { tolerance }), /tolerance/i);
  assert.throws(() => simplifyGeometry(source, null), /options/i);
  assert.throws(() => simplifyGeometry(source, { tolerance: 1, prefix: '' }), /prefix/i);
  assert.throws(() => simplifyGeometry(null, { tolerance: 1 }), /Path input/);

  const curved = structuredClone(source);
  curved.contours[0].points[1].handleA[0] = 0.001;
  assert.throws(() => simplifyGeometry(curved, { tolerance: 1 }), /Resample Path first/);
  assert.deepEqual(
    simplifyGeometry(curved, { tolerance: 0 }).contours[0].points.map((item) => item.handleA),
    curved.contours[0].points.map((item) => item.handleA),
  );

  const invalid = structuredClone(source);
  invalid.contours[0].points[1].position[0] = Infinity;
  assert.throws(() => simplifyGeometry(invalid, { tolerance: 1 }), /finite/i);
  assert.throws(
    () =>
      simplifyGeometry(
        { contours: Array.from({ length: 10001 }, (_, index) => contour(`c${index}`, [])) },
        { tolerance: 1 },
      ),
    /budget/i,
  );
  assert.throws(
    () =>
      simplifyGeometry(
        {
          contours: [
            contour(
              'many',
              Array.from({ length: 100001 }, () => [0, 0]),
            ),
          ],
        },
        { tolerance: 1 },
      ),
    /budget/i,
  );
});

test('rejects non-finite intermediate coordinate arithmetic', () => {
  const max = Number.MAX_VALUE;
  const overflow = pathFromContours([
    contour('overflow', [
      [-max, 0],
      [0, 1],
      [max, 0],
    ]),
  ]);
  assert.throws(
    () => simplifyGeometry(overflow, { tolerance: 1 }),
    /finite-coordinate arithmetic/i,
  );
});

test('rejects distance-work exhaustion before returning partial geometry', () => {
  const count = 100000;
  const nestedCurve = pathFromContours([
    contour(
      'nested-curve',
      Array.from({ length: count }, (_, index) => {
        const x = index / (count - 1);
        return [x, 2 ** (1000 * (x - 1))];
      }),
    ),
  ]);

  assert.throws(
    () => simplifyGeometry(nestedCurve, { tolerance: Number.MIN_VALUE }),
    /distance-work budget/i,
  );
});
