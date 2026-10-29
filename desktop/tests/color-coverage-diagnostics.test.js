import test from 'node:test';
import assert from 'node:assert/strict';
import { summarizeCoverageRasters as summarize } from './helpers/color-coverage-diagnostics.js';

test('diagnostics retain surplus/deficit, missing mass and bounded worst/probe pixels', () => {
  const result = summarize(
    [1, 1, 0.5, 0],
    [
      [0.75, 0, 0.25, 0],
      [0.75, 0, 0.25, 0],
      [0, 0, 0, 0],
      [0, 0, 0, 0],
    ],
    2,
    2,
    [
      [0, 0],
      [1, 0],
    ],
  );
  assert.equal(result.fullMass, 2.5);
  assert.deepEqual(result.componentMasses, [1, 1, 0, 0]);
  assert.equal(result.componentSumMass, 2);
  assert.equal(result.maxAbsoluteDifference, 1);
  assert.equal(result.beyondQuantizationPixels, 2);
  assert.equal(result.missingPositiveMassPixels, 1);
  assert.equal(result.surplusPixels, 1);
  assert.equal(result.deficitPixels, 1);
  assert.deepEqual(
    result.worstPixels.map((p) => p.pixel),
    [1, 0],
  );
  assert.equal(result.probes[0].difference, 0.5);
});

test('two overlapping opaque production wedges cannot be attributed by changing their alpha', () => {
  // A concrete source-over counterexample: each wedge has full alpha 1 and
  // equal adjacent-stop fractions. Replacing their alpha with 0.5 produces
  // two component patterns of 0.75 alpha each, but full paint alpha is only 1.
  const full = 1 - (1 - 1) ** 2;
  const component = 1 - (1 - 0.5) ** 2;
  const result = summarize([full], [[component], [component], [0], [0]], 1, 1);
  assert.equal(result.componentSumMass, 1.5);
  assert.equal(result.maxAbsoluteDifference, 0.5);
  assert.equal(result.beyondQuantizationPixels, 1);
});

test('diagnostics never return unbounded pixel dumps or mutate input masks', () => {
  const full = Array(100).fill(1),
    masks = Array.from({ length: 4 }, () => Array(100).fill(1));
  const before = JSON.stringify([full, masks]);
  const result = summarize(full, masks, 10, 10);
  assert.equal(result.worstPixels.length, 16);
  assert.deepEqual(
    result.worstPixels.map((p) => p.pixel),
    Array.from({ length: 16 }, (_, i) => i),
  );
  assert.equal(JSON.stringify([full, masks]), before);
});

test('malformed diagnostics fail explicitly rather than emitting misleading evidence', () => {
  for (const args of [
    [[], [], 1, 1],
    [[1], [[0], [0], [0], [NaN]], 1, 1],
    [[1], [[0], [0], [0], [0]], 257, 1],
    [[1], [[0], [0], [0], [0]], 1, 1, [[1, 0]]],
  ])
    assert.throws(() => summarize(...args));
});
