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

test('gradient stop masses share one paint alpha, not two mutually occluding layers', () => {
  const split = { startSlotId: 'start', endSlotId: 'end', startAlpha: [0.375, 0.125] };
  const layers = [layer('back', [1, 1]), { alpha: [0.5, 0.5], split }];
  const before = JSON.stringify(layers);
  const result = measure(['start', 'end', 'back'], layers, 2);
  assert.deepEqual(percentages(result), { start: 25, end: 25, back: 50 });
  assert.equal(result.paintedPixelEquivalent, 2);
  assert.equal(JSON.stringify(layers), before);
  const covered = measure(['start', 'end', 'back'], [...layers, layer('back', [1, 0])], 2);
  assert.deepEqual(percentages(covered), { start: 6, end: 19, back: 75 });
});

test('same-ID, unbound, missing and fully transparent gradient stops conserve paint mass', () => {
  const gradient = (a, b, alpha, startAlpha) => ({
    alpha,
    split: { startSlotId: a, endSlotId: b, startAlpha },
  });
  assert.deepEqual(percentages(measure(['same'], [gradient('same', 'same', [0.5], [0.2])], 1)), {
    same: 100,
  });
  const other = measure(['start'], [gradient('start', null, [0.5], [0.125])], 1);
  assert.equal(other.slots[0].percent, 25);
  assert.equal(other.other.percent, 75);
  assert.equal(measure(['start'], [gradient('missing', null, [0.5], [0.2])], 1).other.percent, 100);
  assert.equal(
    measure(['start'], [gradient('start', null, [0], [0])], 1).paintedPixelEquivalent,
    0,
  );
});

test('invalid stop masses or identities reject even when fully occluded', () => {
  for (const split of [
    null,
    {},
    [],
    { startSlotId: 1, endSlotId: null, startAlpha: [0.1] },
    { startSlotId: 'start', endSlotId: '', startAlpha: [0.1] },
    { startSlotId: 'start', endSlotId: null, startAlpha: [] },
    { startSlotId: 'start', endSlotId: null, startAlpha: [NaN] },
    { startSlotId: 'start', endSlotId: null, startAlpha: [0.6] },
  ])
    assert.throws(() => measure(['start'], [{ alpha: [0.5], split }, layer('start', [1])], 1));
  assert.throws(
    () =>
      measure(
        ['start'],
        [
          {
            slotId: 'start',
            alpha: [1],
            split: { startSlotId: 'start', endSlotId: null, startAlpha: [0.5] },
          },
        ],
        1,
      ),
    /attribution/,
  );
});

const fourStops = (ids, masses, alpha = [0.5, 0.5]) => ({
  alpha,
  contributions: ids.map((slotId, i) => ({ slotId, alpha: masses[i] })),
});
test('four angular stop masses compose as one paint and preserve identity, Other and conservation', () => {
  const angular = fourStops(
    ['a', 'b', null, 'missing'],
    [
      [0.1, 0],
      [0.2, 0.1],
      [0.2, 0.1],
      [0, 0.3],
    ],
  );
  const input = [layer('back', [1, 1]), angular, layer('front', [0.5, 0])];
  const before = JSON.stringify(input);
  const result = measure(['a', 'b', 'back', 'front'], input, 2);
  const mass = Object.fromEntries(result.slots.map((row) => [row.id, row.contribution]));
  assert.ok(Math.abs(mass.a - 0.05) < 1e-12);
  assert.ok(Math.abs(mass.b - 0.2) < 1e-12);
  assert.equal(mass.back, 0.75);
  assert.equal(mass.front, 0.5);
  assert.equal(result.other.contribution, 0.5);
  assert.equal(result.paintedPixelEquivalent, 2);
  assert.equal(
    result.slots.reduce((sum, row) => sum + row.percent, result.other.percent),
    100,
  );
  assert.equal(JSON.stringify(input), before);
  const same = fourStops(Array(4).fill('same'), [[0.1], [0.2], [0], [0.2]], [0.5]);
  assert.equal(measure(['same'], [same], 1).slots[0].percent, 100);
  const zero = fourStops(
    Array(4).fill(null),
    Array.from({ length: 4 }, () => [0]),
    [0],
  );
  assert.equal(measure([], [zero], 1).paintedPixelEquivalent, 0);
});

test('malformed four-stop attribution fails even under complete occlusion', () => {
  const valid = fourStops(['a', null, null, null], [[0.5], [0], [0], [0]], [0.5]);
  for (const patch of [
    { contributions: null },
    { contributions: [] },
    { contributions: new Array(4) },
    { contributions: [...valid.contributions.slice(0, 3), { slotId: 'bad id', alpha: [0] }] },
    { contributions: [...valid.contributions.slice(0, 3), { slotId: null, alpha: [0.1] }] },
    { contributions: [...valid.contributions.slice(0, 3), { slotId: null, alpha: [NaN] }] },
    { slotId: 'a' },
    { split: { startSlotId: 'a', endSlotId: null, startAlpha: [0.5] } },
  ])
    assert.throws(() => measure(['a'], [{ ...valid, ...patch }, layer('a', [1])], 1));
});
