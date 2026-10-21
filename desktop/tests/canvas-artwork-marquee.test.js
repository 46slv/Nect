import test from 'node:test';
import assert from 'node:assert/strict';
import { artworkMarqueeIds } from '../src/canvas-artwork-marquee.js';
const targets = [
  {
    id: 'a',
    bounds: [
      [10, 20, 30],
      [10, 20, 30],
    ],
  },
  {
    id: 'b',
    bounds: [
      [60, 70, 80],
      [10, 20, 30],
    ],
  },
  {
    id: 'line',
    bounds: [
      [90, 90, 90],
      [5, 20, 35],
    ],
  },
];
test('artwork marquee uses inclusive AABB overlap, reverse drags and line bounds', () => {
  assert.deepEqual(artworkMarqueeIds(targets, [40, 40], [20, 20]), ['a']);
  assert.deepEqual(artworkMarqueeIds(targets, [30, 30], [30, 40]), ['a']);
  assert.deepEqual(artworkMarqueeIds(targets, [0, 0], [100, 6]), ['line']);
  assert.deepEqual(artworkMarqueeIds(targets, [31, 0], [59, 40]), []);
});
test('Shift union always uses the immutable starting set', () => {
  const before = ['b'],
    snapshot = structuredClone(targets);
  assert.deepEqual(artworkMarqueeIds(targets, [0, 0], [40, 40], before), ['b', 'a']);
  assert.deepEqual(artworkMarqueeIds(targets, [0, 0], [5, 5], before), ['b']);
  assert.deepEqual(before, ['b']);
  assert.deepEqual(targets, snapshot);
});
