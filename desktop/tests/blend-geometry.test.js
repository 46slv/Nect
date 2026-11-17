import test from 'node:test';
import assert from 'node:assert/strict';
import { interpolateClosedContours } from '../src/blend-geometry.js';
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
