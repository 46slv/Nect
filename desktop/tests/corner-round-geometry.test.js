import test from 'node:test';
import assert from 'node:assert/strict';
import { roundCorners } from '../src/corner-round-geometry.js';

const point = (id, position, handleA = [0, 0], handleB = [0, 0]) => ({
  id,
  position,
  handleA,
  handleB,
});
const contour = (positions, closed = true, extra = {}) => ({
  id: 'source-contour',
  closed,
  ...extra,
  points: positions.map((position, i) => point(`source-${i}`, position)),
});
const near = (actual, expected, epsilon = 1e-8) =>
  actual.forEach((value, i) =>
    assert.ok(Math.abs(value - expected[i]) <= epsilon, `${actual} != ${expected}`),
  );

test('rounds rectangle corners with tangent circular cubic arcs and deterministic IDs', () => {
  const source = {
    contours: [
      contour([
        [0, 0],
        [10, 0],
        [10, 10],
        [0, 10],
      ]),
    ],
  };
  const rounded = roundCorners(source, { radius: 2, prefix: 'r' });
  const points = rounded.contours[0].points;
  assert.equal(points.length, 8);
  near(points[0].position, [0, 2]);
  near(points[1].position, [2, 0]);
  near(points[2].position, [8, 0]);
  near(points[3].position, [10, 2]);
  near(points[0].handleB, [0, -(4 / 3) * (Math.sqrt(2) - 1) * 2]);
  near(points[1].handleA, [-(4 / 3) * (Math.sqrt(2) - 1) * 2, 0]);
  assert.deepEqual(
    points.map(({ id }) => id),
    ['r:c0:p0', 'r:c0:p1', 'r:c0:p2', 'r:c0:p3', 'r:c0:p4', 'r:c0:p5', 'r:c0:p6', 'r:c0:p7'],
  );
  assert.deepEqual(roundCorners(source, { radius: 2, prefix: 'r' }), rounded);
});

test('caps each tangent distance to half its adjacent edges', () => {
  const rounded = roundCorners(
    {
      contours: [
        contour([
          [0, 0],
          [10, 0],
          [10, 10],
          [0, 10],
        ]),
      ],
    },
    { radius: 100, prefix: 'cap' },
  );
  const points = rounded.contours[0].points;
  near(points[2].position, [5, 0]);
  near(points[3].position, [10, 5]);
  near(points[0].position, [0, 5]);
  near(points[1].position, [5, 0]);
});

test('rounds concave corners with the signed turn and keeps the resulting arc on the correct side', () => {
  const source = {
    contours: [
      contour([
        [0, 0],
        [10, 0],
        [10, 10],
        [5, 5],
        [0, 10],
      ]),
    ],
  };
  const points = roundCorners(source, { radius: 1, prefix: 'concave' }).contours[0].points;
  const start = points.find((p) => Math.abs(p.position[0] - (5 + Math.SQRT1_2)) < 1e-8);
  const end = points.find((p) => Math.abs(p.position[0] - (5 - Math.SQRT1_2)) < 1e-8);
  near(start.position, [5 + Math.SQRT1_2, 5 + Math.SQRT1_2]);
  near(end.position, [5 - Math.SQRT1_2, 5 + Math.SQRT1_2]);
  const cubicMidpoint = start.position.map(
    (value, i) =>
      0.125 * value +
      0.375 * (value + start.handleB[i]) +
      0.375 * (end.position[i] + end.handleA[i]) +
      0.125 * end.position[i],
  );
  assert.ok(
    cubicMidpoint[1] > 5,
    `concave arc should curve below its sharp vertex: ${cubicMidpoint}`,
  );
});

test('preserves open endpoints and leaves collinear vertices as straight points', () => {
  const open = roundCorners(
    {
      contours: [
        contour(
          [
            [0, 0],
            [10, 0],
            [10, 10],
          ],
          false,
        ),
      ],
    },
    { radius: 2, prefix: 'open' },
  ).contours[0];
  near(open.points[0].position, [0, 0]);
  near(open.points.at(-1).position, [10, 10]);
  assert.equal(open.closed, false);

  const withCollinear = roundCorners(
    {
      contours: [
        contour(
          [
            [0, 0],
            [5, 0],
            [10, 0],
            [10, 10],
          ],
          false,
        ),
      ],
    },
    { radius: 1, prefix: 'line' },
  ).contours[0];
  assert.ok(withCollinear.points.some((p) => p.position[0] === 5 && p.position[1] === 0));
  assert.ok(
    withCollinear.points.every((p) =>
      [...p.position, ...p.handleA, ...p.handleB].every(Number.isFinite),
    ),
  );
});

test('zero radius returns a geometry-equivalent independent clone, including curved handles', () => {
  const source = {
    name: 'source',
    contours: [
      {
        ...contour(
          [
            [0, 0],
            [4, 0],
          ],
          false,
          { group: 'shape-a' },
        ),
        points: [point('a', [0, 0], [1, 2], [3, 4]), point('b', [4, 0])],
      },
    ],
  };
  const copy = roundCorners(source, { radius: 0 });
  assert.deepEqual(copy, source);
  assert.notEqual(copy, source);
  assert.notEqual(copy.contours[0], source.contours[0]);
  assert.notEqual(copy.contours[0].points[0].position, source.contours[0].points[0].position);
  copy.contours[0].points[0].position[0] = 42;
  assert.equal(source.contours[0].points[0].position[0], 0);
});

test('preserves contour group membership and winding order', () => {
  const source = {
    contours: [
      contour(
        [
          [0, 0],
          [10, 0],
          [10, 10],
          [0, 10],
        ],
        true,
        { group: 'compound-1' },
      ),
      contour(
        [
          [2, 2],
          [2, 8],
          [8, 8],
          [8, 2],
        ],
        true,
        { group: 'compound-1' },
      ),
    ],
  };
  const rounded = roundCorners(source, { radius: 1, prefix: 'grouped' });
  assert.deepEqual(
    rounded.contours.map((c) => c.group),
    ['compound-1', 'compound-1'],
  );
  for (let i = 0; i < source.contours.length; i++) {
    const area = (points) =>
      points.reduce((sum, p, index) => {
        const next = points[(index + 1) % points.length];
        return sum + p.position[0] * next.position[1] - next.position[0] * p.position[1];
      }, 0);
    assert.equal(
      Math.sign(area(rounded.contours[i].points)),
      Math.sign(area(source.contours[i].points)),
    );
  }
});

test('rejects malformed radius, zero-length line edges and non-finite geometry', () => {
  for (const radius of [-1, Infinity, NaN])
    assert.throws(() => roundCorners({ contours: [] }, { radius }), /finite and nonnegative/);
  assert.throws(
    () =>
      roundCorners(
        {
          contours: [
            contour([
              [0, 0],
              [0, 0],
              [1, 1],
            ]),
          ],
        },
        { radius: 1 },
      ),
    /zero-length edge/,
  );
  const invalid = {
    contours: [
      contour(
        [
          [0, 0],
          [1, 0],
        ],
        false,
      ),
    ],
  };
  invalid.contours[0].points[1].position[1] = Infinity;
  assert.throws(() => roundCorners(invalid, { radius: 0 }), /finite point positions/);
});

test('rejects outputs that exceed the geometry point budget', () => {
  const positions = Array.from({ length: 50002 }, (_, i) => [i, i % 2]);
  assert.throws(
    () => roundCorners({ contours: [contour(positions, false)] }, { radius: 1, prefix: 'large' }),
    /geometry budget/,
  );
});

test('mixed contours round only line-line corners and retain cubic endpoints and handles exactly', () => {
  const source = {
    contours: [
      {
        id: 'mixed',
        closed: false,
        group: 'g',
        points: [
          point('a', [0, 0], [8, 7]),
          point('b', [10, 0]),
          point('c', [10, 10]),
          point('d', [20, 10], [0, 0], [4, -6]),
          point('e', [30, 20], [-7, 2]),
          point('f', [40, 20], [0, 0], [5, 9]),
        ],
      },
    ],
  };
  const original = structuredClone(source);
  const result = roundCorners(source, { radius: 2 });
  const points = result.contours[0].points;
  assert.equal(points.length, 8); // b and c rounded; d/e curve-adjacent, a/f open ends
  for (const index of [0, 3, 4, 5]) {
    const expected = source.contours[0].points[index];
    const actual = points.find(
      (p) => JSON.stringify(p.position) === JSON.stringify(expected.position),
    );
    assert.ok(actual);
    assert.deepEqual(actual.handleA, expected.handleA);
    assert.deepEqual(actual.handleB, expected.handleB);
    assert.notEqual(actual.position, expected.position);
    assert.notEqual(actual.handleB, expected.handleB);
  }
  assert.equal(result.contours[0].group, 'g');
  assert.deepEqual(source, original);
});

test('both remote handles prevent false line-line eligibility, including the closed seam', () => {
  for (const [pointIndex, handle] of [
    [0, 'handleB'],
    [2, 'handleA'],
  ]) {
    const c = contour(
      [
        [0, 0],
        [10, 0],
        [10, 10],
      ],
      false,
    );
    c.points[pointIndex][handle] = [1, 2];
    assert.equal(roundCorners({ contours: [c] }, { radius: 2 }).contours[0].points.length, 3);
  }
  const c = contour([
    [0, 0],
    [10, 0],
    [10, 10],
    [0, 10],
  ]);
  c.points[3].handleB = [3, 0];
  const result = roundCorners({ contours: [c] }, { radius: 2 }).contours[0];
  assert.equal(result.points.length, 6);
  for (const i of [0, 3]) {
    const p = result.points.find((p) => p.position.every((v, j) => v === c.points[i].position[j]));
    assert.deepEqual(p.handleB, c.points[i].handleB);
  }
});

test('short closed cubic contours and coincident cubic endpoints remain intact', () => {
  const c = {
    closed: true,
    points: [point('a', [0, 0], [-2, 3], [2, 3]), point('b', [0, 0], [2, -3], [-2, -3])],
  };
  const result = roundCorners({ contours: [c] }, { radius: 2 });
  assert.deepEqual(
    result.contours[0].points.map(({ id, ...p }) => p),
    c.points.map(({ id, ...p }) => p),
  );
});
