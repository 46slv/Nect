import test from 'node:test';
import assert from 'node:assert/strict';
import { measureColorCoverage as measure } from '../src/color-coverage.js';
const layer = (slotId, alpha) => ({ slotId, alpha });
const percentages = (result) => Object.fromEntries(result.slots.map((s) => [s.id, s.percent]));

test('empty and transparent artboards have zero percentages, not a white background', () => {
  for (const result of [measure(['c1'], [], 0), measure(['c1'], [layer('c1', [0, 0])], 2)]) {
    assert.deepEqual(percentages(result), { c1: 0 });
    assert.equal(result.other.percent, 0);
    assert.equal(result.paintedPixelEquivalent, 0);
  }
  const result = measure(['c1'], [layer('c1', [1, 0, 0, 0])], 4);
  assert.equal(result.slots[0].percent, 100);
  assert.equal(result.paintedPixelEquivalent, 1);
});

test('occlusion and partial alpha count visible contribution in document paint order', () => {
  const result = measure(['back', 'front'], [layer('back', [1, 1]), layer('front', [1, 0.5])], 2);
  assert.deepEqual(percentages(result), { back: 25, front: 75 });
  assert.equal(result.paintedPixelEquivalent, 2);
  const reversed = measure(['back', 'front'], [layer('front', [1, 0.5]), layer('back', [1, 1])], 2);
  assert.deepEqual(percentages(reversed), { back: 100, front: 0 });
});

test('unlinked or missing references stay Other; stable IDs never merge by appearance', () => {
  const result = measure(
    ['c1', 'c2'],
    [
      layer('c1', [1, 0, 0, 0]),
      layer('c2', [0, 1, 0, 0]),
      layer(null, [0, 0, 1, 0]),
      layer('missing-id', [0, 0, 0, 1]),
    ],
    4,
  );
  assert.deepEqual(percentages(result), { c1: 25, c2: 25 });
  assert.equal(result.other.percent, 50);
});

test('integer allocation sums to 100 with ID-stable ties, not a composition quota', () => {
  const layers = [layer('c1', [1, 0, 0]), layer('c2', [0, 1, 0]), layer('c3', [0, 0, 1])];
  assert.deepEqual(percentages(measure(['c1', 'c2', 'c3'], layers, 3)), { c1: 34, c2: 33, c3: 33 });
  assert.deepEqual(percentages(measure(['c3', 'c2', 'c1'], layers, 3)), { c3: 33, c2: 33, c1: 34 });
});

test('typed alpha masks and repeated same-ID objects do not mutate inputs', () => {
  const mask = new Float32Array([0.25, 0.5]);
  const layers = [layer('c1', mask), layer('c1', [0.5, 0.5])];
  const before = [...mask];
  const result = measure(['c1'], layers, 2);
  assert.equal(result.slots[0].percent, 100);
  assert.equal(result.paintedPixelEquivalent, 1.375);
  assert.deepEqual([...mask], before);
});

test('malformed and over-budget inputs reject, including invalid hidden layers', () => {
  for (const args of [
    [['c1', 'c1'], [], 0],
    [[''], [], 0],
    [['c1'], [], -1],
    [['c1'], [], 262145],
    [['c1'], [layer('c1', [NaN]), layer('c1', [1])], 1],
    [['c1'], [layer('c1', [-0.1])], 1],
    [['c1'], [layer('c1', [1.1])], 1],
    [['c1'], [layer('c1', [])], 1],
    [['c1'], [layer({}, [1])], 1],
    [['c1'], Array.from({ length: 257 }, () => layer('c1', [])), 0],
  ])
    assert.throws(() => measure(...args));
});

test('sparse layer buffers reject even when fully occluded or empty', () => {
  const sparse = new Array(2);
  sparse[1] = layer('c1', [1]);
  assert.throws(() => measure(['c1'], sparse, 1), /matching normalized alpha buffer/);
  assert.throws(() => measure(['c1'], new Array(1), 0), /matching normalized alpha buffer/);
});
