import test from 'node:test';
import assert from 'node:assert/strict';
import { polarTransformGeometry } from '../src/polar-transform.js';

const settings = {
  origin: [0, 0],
  center: [0, 0],
  width: 100,
  height: 100,
  radius: 100,
  angle: 0,
  inverse: true,
};
const point = (position, handleA = [0, 0], handleB = [0, 0]) => ({
  position,
  handleA,
  handleB,
});
const path = (points, closed = false) => ({ contours: [{ closed, points }] });
const near = (actual, expected) =>
  actual.forEach((value, i) => assert.ok(Math.abs(value - expected[i]) < 1e-6));

test('inverse refuses a cubic that crosses the seam twice between its endpoints', () => {
  // Both anchors are on the opposite side of the seam. Their controls are
  // individually within half a turn of their anchors, but the cubic midpoint
  // lies on the positive X seam ray, so the curve crosses it twice.
  const source = path([
    point([-1, 0], [0, 0], [1.9510565, 0.309017]),
    point([-1, 0], [1.9510565, -0.309017]),
  ]);
  const before = structuredClone(source);
  assert.throws(() => polarTransformGeometry(source, settings), /seam/);
  assert.deepEqual(source, before);
});

test('inverse also checks the closing cubic of a closed contour', () => {
  const source = path(
    [
      point([-1, 0], [1.9510565, -0.309017]),
      point([0, -1]),
      point([-1, 0], [0, 0], [1.9510565, 0.309017]),
    ],
    true,
  );
  const before = structuredClone(source);
  assert.throws(() => polarTransformGeometry(source, settings), /seam/);
  assert.deepEqual(source, before);
});

test('inverse preserves safe cubic anchors and maps controls as relative offsets', () => {
  const source = path([point([-1, 0], [0, 0], [0, 1]), point([-1, 1], [0, -1])]);
  const before = structuredClone(source);
  const points = polarTransformGeometry(source, settings).contours[0].points;
  near(points[0].position, [50, 1]);
  near(points[0].handleB, [-12.5, Math.SQRT2 - 1]);
  near(points[1].position, [37.5, Math.SQRT2]);
  near(points[1].handleA, [12.5, 1 - Math.SQRT2]);
  assert.deepEqual(source, before);
});

test('forward mapping retains safe relative cubic control behavior', () => {
  const source = path([point([25, 100], [0, 0], [25, 0]), point([75, 100], [-25, 0])]);
  const before = structuredClone(source);
  const points = polarTransformGeometry(source, { ...settings, inverse: false }).contours[0].points;
  near(points[0].position, [0, 100]);
  near(points[0].handleB, [-100, -100]);
  near(points[1].position, [0, -100]);
  near(points[1].handleA, [-100, 100]);
  assert.deepEqual(source, before);
});
