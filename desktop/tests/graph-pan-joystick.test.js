import test from 'node:test';
import assert from 'node:assert/strict';
import {
  joystickVector,
  joystickMotion,
  joystickLayout,
  joystickOverviewLayout,
  JOYSTICK_SPEED,
} from '../src/graph-pan-joystick.js';
const near = (actual, expected) =>
  actual.forEach((v, i) => assert.ok(Math.abs(v - expected[i]) < 1e-8, `${actual} vs ${expected}`));
test('joystick direction, radial bounds, dead zone and continuous speed', () => {
  assert.deepEqual(joystickVector(0, 0, 18), { knob: [0, 0], velocity: [0, 0] });
  assert.deepEqual(joystickVector(NaN, 0, 18).velocity, [0, 0]);
  assert.deepEqual(joystickVector(1, 1, 0).knob, [0, 0]);
  for (const [x, y] of [
    [18, 0],
    [-18, 0],
    [0, 18],
    [0, -18],
    [900, -900],
  ]) {
    const { knob, velocity } = joystickVector(x, y, 18);
    assert.ok(Math.hypot(...knob) <= 18 + 1e-8);
    near(
      velocity,
      [x, y].map((v) => v / Math.hypot(x, y)),
    );
  }
  assert.deepEqual(joystickVector(2, 0, 18).velocity, [0, 0]);
  near(joystickVector(9, 0, 18).velocity, [(0.5 - 0.12) / 0.88, 0]);
  assert.ok(joystickVector(10, 0, 18).velocity[0] > joystickVector(9, 0, 18).velocity[0]);
});
test('elapsed-time motion is equal at 30, 60 and 144 Hz with bounded stalled frames', () => {
  for (const hz of [30, 60, 144]) {
    const total = Array.from({ length: hz }, () => joystickMotion([1, -0.5], 1000 / hz)).reduce(
      (a, b) => a.map((v, i) => v + b[i]),
      [0, 0],
    );
    near(total, [JOYSTICK_SPEED, -JOYSTICK_SPEED / 2]);
  }
  near(joystickMotion([1, 1], 1000), [30, 30]);
  near(joystickMotion([1, 1], -10), [0, 0]);
});
test('joystick remains inside short/narrow viewport and clear of native scrollbars', () => {
  for (const [width, height] of [
    [800, 300],
    [400, 80],
    [180, 60],
    [120, 40],
  ]) {
    const { size, right, bottom } = joystickLayout(width, height, 17, 17);
    assert.ok(size <= 80 && size <= Math.min(width, height) - 16);
    assert.ok(right >= 25 && bottom >= 25);
    assert.ok(size + right <= width + 17 - 8);
    assert.ok(size + bottom <= height + 17 - 8);
  }
  assert.equal(joystickLayout(800, 300).size, 80);
  assert.equal(joystickLayout(400, 60).size, 44);
});

test('Overview clears joystick including the exact compact-height boundary and borders', () => {
  for (const width of [180, 272, 400, 800])
    for (const height of [40, 60, 80, 214, 215, 216, 217, 224, 225, 226, 300]) {
      const stick = joystickLayout(width, height, 17, 17);
      const overview = joystickOverviewLayout(width, height, stick);
      const stickLeft = width + 17 - stick.right - stick.size;
      const stickTop = height + 17 - stick.bottom - stick.size;
      const overviewRight = width + 17 - overview.right;
      const overviewBottom = 8 + Math.max(0, overview.width - 2) / 2 + 2;
      assert.ok(
        overviewRight + 8 <= stickLeft || overviewBottom + 8 <= stickTop,
        `${width}x${height} overlaps`,
      );
    }
});
