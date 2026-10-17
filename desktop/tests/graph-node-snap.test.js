import test from 'node:test';
import assert from 'node:assert/strict';
import { snapGraphNodes } from '../src/layout.js';
const box = (id, x, y, width = 100, height = 80) => ({ id, x, y, width, height });
test('graph snapping uses nearest edges/centers and screen-pixel tolerance at every zoom', () => {
  for (const zoom of [0.05, 0.5, 1, 2]) {
    const source = box('a', 400, 400, 10, 10),
      target = box('b', 800, 800, 10, 10);
    const hit = snapGraphNodes([source], [target], [400 - 5 / zoom, 400], zoom);
    assert.ok(hit.guides.some((g) => g.axis === 0));
    assert.ok(Math.abs(hit.delta[0] - (400 - 5 / zoom)) * zoom <= 6);
    const miss = snapGraphNodes([source], [target], [400 + 10 + 7 / zoom, 400], zoom);
    assert.equal(
      miss.guides.some((g) => g.axis === 0),
      false,
    );
  }
  const centered = snapGraphNodes([box('a', 100, 100)], [box('b', 400, 500, 200)], [346, 0], 1);
  assert.equal(centered.delta[0], 350);
  assert.equal(centered.guides[0].coordinate, 500);
});
test('group translation excludes itself, preserves spacing, clamps once, and bypasses cleanly', () => {
  const moving = [box('a', 20, 30), box('b', 180, 230)];
  assert.deepEqual(snapGraphNodes(moving, moving, [4, 5], 1), { delta: [4, 5], guides: [] });
  assert.deepEqual(snapGraphNodes(moving, [box('c', 21, 32)], [-100, -100], 1, true), {
    delta: [-20, -30],
    guides: [],
  });
  const result = snapGraphNodes(moving, [box('c', 300, 500)], [117, 267], 1);
  assert.deepEqual(result.delta, [120, 270]);
  assert.equal(result.guides.length, 2);
  assert.deepEqual(snapGraphNodes([], [], [4, 5], 1), { delta: [4, 5], guides: [] });
});
test('ties are deterministic regardless of target order and origin-invalid alignments are rejected', () => {
  const moving = [box('a', 1, 30)],
    targets = [box('z', 98, 500), box('b', 104, 600)];
  assert.deepEqual(
    snapGraphNodes(moving, targets, [0, 0], 1),
    snapGraphNodes(moving, targets.reverse(), [0, 0], 1),
  );
  const result = snapGraphNodes(moving, [box('c', 0, 400, 99)], [-50, 0], 1);
  assert.ok(result.delta[0] >= -1);
  for (const g of result.guides) assert.ok(Number.isFinite(g.coordinate));
});
