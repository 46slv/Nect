import assert from 'node:assert/strict';
import test from 'node:test';
import {
  ARTBOARD_UNITS,
  CSS_PIXELS_PER_INCH,
  MILLIMETRES_PER_INCH,
  displayLengthToDocumentUnits,
  documentUnitsToDisplayLength,
  formatDocumentLength,
} from '../src/artboard-units.js';

const close = (actual, expected, tolerance = 1e-12) =>
  assert.ok(Math.abs(actual - expected) <= tolerance * Math.max(1, Math.abs(expected)));

test('artboard units use the 96 CSS px per inch reference', () => {
  assert.equal(CSS_PIXELS_PER_INCH, 96);
  assert.equal(MILLIMETRES_PER_INCH, 25.4);
  close(displayLengthToDocumentUnits(25.4, 'mm'), 96);
  close(displayLengthToDocumentUnits(2.54, 'cm'), 96);
  close(displayLengthToDocumentUnits(100, 'mm'), (100 * 96) / 25.4);
  assert.equal(formatDocumentLength(displayLengthToDocumentUnits(100, 'mm'), 'mm'), '100');
});

test('unit conversions round-trip without changing canonical document pixels', () => {
  for (const documentPixels of [1, 96, 377.9527559055118, 960, 100000]) {
    for (const unit of ARTBOARD_UNITS) {
      const displayed = documentUnitsToDisplayLength(documentPixels, unit);
      close(displayLengthToDocumentUnits(displayed, unit), documentPixels);
    }
  }
});

test('px remains an exact identity for existing documents and commands', () => {
  for (const pixels of [0, 1, 321.5, 960, 100000]) {
    assert.equal(documentUnitsToDisplayLength(pixels, 'px'), pixels);
    assert.equal(displayLengthToDocumentUnits(pixels, 'px'), pixels);
    assert.equal(formatDocumentLength(pixels, 'px'), String(pixels));
  }
});

test('invalid dimensions and unsupported units fail closed', () => {
  for (const value of [
    '',
    'not-a-number',
    '0x10',
    '1.5mm',
    null,
    true,
    NaN,
    Infinity,
    -Infinity,
    Number.MAX_VALUE,
  ])
    assert.throws(() => displayLengthToDocumentUnits(value, 'cm'));
  assert.throws(() => documentUnitsToDisplayLength(NaN, 'mm'));
  assert.throws(() => displayLengthToDocumentUnits(10, 'in'));
  assert.throws(() => displayLengthToDocumentUnits(10, 'toString'));
  assert.throws(() => formatDocumentLength(10, 'in'));
});
