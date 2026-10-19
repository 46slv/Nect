import assert from 'node:assert/strict';
import test from 'node:test';
import { displayLengthToDocumentUnits } from '../src/artboard-units.js';
import { pngDimensions } from '../src/raster.js';

const mm = (value) => displayLengthToDocumentUnits(value, 'mm');
const cm = (value) => displayLengthToDocumentUnits(value, 'cm');

test('PPI output rounds physical document dimensions to whole pixels', () => {
  assert.deepEqual(pngDimensions({ width: mm(100), height: mm(50) }, { mode: 'ppi', ppi: 300 }), {
    width: 1181,
    height: 591,
  });
  assert.deepEqual(pngDimensions({ width: cm(10), height: cm(5) }, { mode: 'ppi', ppi: 300 }), {
    width: 1181,
    height: 591,
  });
});

test('A4 at 300 PPI uses deterministic nearest-pixel rounding', () => {
  assert.deepEqual(pngDimensions({ width: mm(210), height: mm(297) }, { mode: 'ppi', ppi: 300 }), {
    width: 2480,
    height: 3508,
  });
});

test('existing numeric scale behavior and explicit scale mode remain unchanged', () => {
  const doc = { width: 321.5, height: 160.25 };
  assert.deepEqual(pngDimensions(doc), { width: 322, height: 160 });
  assert.deepEqual(pngDimensions(doc, 2), { width: 643, height: 321 });
  assert.deepEqual(pngDimensions(doc, { mode: 'scale', scale: 2 }), { width: 643, height: 321 });
});

test('PPI mode is not constrained by the legacy scale preset range', () => {
  assert.deepEqual(pngDimensions({ width: 100, height: 50 }, { mode: 'ppi', ppi: 1200 }), {
    width: 1250,
    height: 625,
  });
});

test('invalid source dimensions, options, and oversized results are rejected without mutation', () => {
  const original = { width: mm(100), height: mm(50), name: 'unchanged' };
  const before = structuredClone(original);

  for (const doc of [
    null,
    {},
    { width: NaN, height: 1 },
    { width: 1, height: Infinity },
    { width: 0, height: 1 },
    { width: -1, height: 1 },
  ])
    assert.throws(() => pngDimensions(doc, { mode: 'ppi', ppi: 300 }));

  for (const ppi of [NaN, Infinity, -Infinity, 0, -1, Number.MAX_VALUE])
    assert.throws(() => pngDimensions(original, { mode: 'ppi', ppi }));
  for (const scale of [NaN, Infinity, 0, -1, 0.09, 8.01])
    assert.throws(() => pngDimensions(original, { mode: 'scale', scale }));
  assert.throws(() => pngDimensions(original, { mode: 'other', scale: 1 }));
  assert.throws(() => pngDimensions(original, { mode: 'ppi', ppi: 300, scale: 1 }));
  assert.throws(() => pngDimensions({ width: 1_000_000, height: 1_000_000 }, 1));

  assert.deepEqual(original, before);
});
