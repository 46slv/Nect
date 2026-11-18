import test from 'node:test';
import assert from 'node:assert/strict';
import { interpolateClosedContours } from '../src/blend-geometry.js';
import { subdividePathGeometry } from '../src/subdivide-geometry.js';
const contour = (positions) => ({
  closed: true,
  points: positions.map((position, i) => ({
    id: String(i),
    position,
    handleA: [-1, 0],
    handleB: [1, 0],
  })),
});
const a = contour([
  [0, 0],
  [4, 0],
  [4, 4],
  [0, 4],
]);
const b = contour([
  [12, 4],
  [12, 0],
  [8, 0],
  [8, 4],
]);
test('aligns reversed winding and seam without mutating inputs', () => {
  const old = JSON.stringify([a, b]);
  const out = interpolateClosedContours(a, b, 0.5, 'x');
  assert.deepEqual(
    out.points.map((p) => p.position),
    [
      [4, 0],
      [8, 0],
      [8, 4],
      [4, 4],
    ],
  );
  assert.deepEqual(out.points[0].handleA, [0, 0]);
  assert.equal(JSON.stringify([a, b]), old);
  assert.equal(out.points[0].id, 'x:p0');
});
test('rejects invalid topology and interpolation', () => {
  assert.throws(() => interpolateClosedContours(a, { ...b, closed: false }, 0.5), /closed/);
  assert.throws(() => interpolateClosedContours(a, b, NaN), /between/);
});
test('rejects ambiguous repeated anchor positions', () => {
  const ambiguous = contour([
    [0, 0],
    [0, 0],
  ]);
  assert.throws(() => interpolateClosedContours(ambiguous, ambiguous, 0.5), /repeated anchor/);
});

test('subdivides mismatched contours while keeping endpoints and sources exact', () => {
  const triangle = contour([
    [0, 0],
    [4, 0],
    [2, 4],
  ]);
  const before = JSON.stringify([triangle, a]);
  const middle = interpolateClosedContours(triangle, a, 0.5);
  assert.equal(middle.points.length, 12);
  for (const [t, source] of [
    [0, triangle],
    [1, a],
  ]) {
    const result = interpolateClosedContours(triangle, a, t);
    assert.deepEqual(
      result.points.map(({ position, handleA, handleB }) => ({ position, handleA, handleB })),
      source.points.map(({ position, handleA, handleB }) => ({ position, handleA, handleB })),
    );
  }
  assert.equal(JSON.stringify([triangle, a]), before);
});

test('rejects subdivision beyond the bounded point budget', () => {
  const many = (n) =>
    contour(
      Array.from({ length: n }, (_, i) => [
        Math.cos((i * 2 * Math.PI) / n),
        Math.sin((i * 2 * Math.PI) / n),
      ]),
    );
  assert.throws(() => interpolateClosedContours(many(31), many(32), 0.5), /budget/);
});

test('rejects finite input coordinates whose alignment arithmetic overflows', () => {
  const huge = contour([
    [1e308, 0],
    [-1e308, 0],
    [0, 1e308],
  ]);
  assert.throws(() => interpolateClosedContours(huge, huge, 0.5), /finite/);
});

test('rejects overflowing handle interpolation without returning invalid geometry', () => {
  const first = structuredClone(a),
    second = structuredClone(a);
  first.points[0].handleA = [1e308, 0];
  second.points[0].handleA = [-1e308, 0];
  assert.throws(() => interpolateClosedContours(first, second, 0.5), /finite/);
});

test('preserves curved segments through unequal counts, reversed winding and shifted seam', () => {
  const source = contour([
    [0, 0],
    [18, 2],
    [5, 13],
  ]);
  source.points[0].handleA = [-3, 2];
  source.points[0].handleB = [4, -5];
  source.points[1].handleA = [-2, -4];
  source.points[1].handleB = [3, 6];
  source.points[2].handleA = [5, 2];
  source.points[2].handleB = [-6, -1];
  const target = subdividePathGeometry({ contours: [source] }, { segments: 2, prefix: 'fixture' })
    .contours[0];
  target.points = target.points.reverse().map((p) => ({
    ...p,
    position: [p.position[0] + 40, p.position[1] - 20],
    handleA: p.handleB,
    handleB: p.handleA,
  }));
  target.points.push(...target.points.splice(0, 2));
  const before = JSON.stringify([source, target]);
  // Independent Bernstein evaluation, not the subdivision implementation as oracle.
  const at = (c, edge, t) => {
    const p = c.points[edge],
      q = c.points[(edge + 1) % c.points.length];
    const u = 1 - t;
    return [0, 1].map(
      (axis) =>
        u ** 3 * p.position[axis] +
        3 * u ** 2 * t * (p.position[axis] + p.handleB[axis]) +
        3 * u * t ** 2 * (q.position[axis] + q.handleA[axis]) +
        t ** 3 * q.position[axis],
    );
  };
  for (const blend of [0.1, 0.5, 0.9]) {
    const result = interpolateClosedContours(source, target, blend);
    assert.equal(result.points.length, 6);
    for (let edge = 0; edge < 6; edge++) {
      for (const t of [0, 0.2, 0.5, 0.8, 1]) {
        const actual = at(result, edge, t);
        const expected = at(source, Math.floor(edge / 2), ((edge % 2) + t) / 2);
        for (let axis = 0; axis < 2; axis++) {
          expected[axis] += [40, -20][axis] * blend;
          assert.ok(
            Math.abs(actual[axis] - expected[axis]) < 1e-9,
            `blend=${blend}, edge=${edge}, t=${t}, axis=${axis}`,
          );
        }
      }
    }
  }
  assert.equal(JSON.stringify([source, target]), before);
});
