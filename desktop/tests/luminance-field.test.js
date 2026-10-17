import test from 'node:test';
import assert from 'node:assert/strict';
import { sampleLuminance, sampleLuminanceGrid } from '../src/luminance-field.js';

const black = { hex: '#000000', alpha: 1 },
  white = { hex: '#ffffff', alpha: 1 },
  bounds = { minX: 0, minY: 0, maxX: 32, maxY: 16 },
  linearizedEncodedMidpoint = ((0.5 + 0.055) / 1.055) ** 2.4;

test('solid sRGB colors use WCAG 2.1 relative luminance', () => {
  assert.equal(sampleLuminance(black, 0, 0), 0);
  assert.equal(sampleLuminance(white, 0, 0), 1);
  assert.equal(sampleLuminance({ hex: '#ff0000', alpha: 1 }, 0, 0), 0.2126);
  assert.equal(sampleLuminance({ hex: '#00ff00', alpha: 1 }, 0, 0), 0.7152);
  assert.equal(sampleLuminance({ hex: '#0000ff', alpha: 1 }, 0, 0), 0.0722);
  assert.ok(
    Math.abs(sampleLuminance({ hex: '#808080', alpha: 1 }, 0, 0) - 0.21586050011389926) < 1e-14,
  );
});

test('two-stop linear gradients project onto their axis and pad outside both stops', () => {
  const fill = {
    kind: 'linear-gradient',
    start: [0, 0],
    end: [100, 0],
    startColor: black,
    endColor: white,
  };
  assert.equal(sampleLuminance(fill, 0, 0), 0);
  assert.equal(sampleLuminance(fill, 100, 0), 1);
  assert.equal(sampleLuminance(fill, -100, 70), 0);
  assert.equal(sampleLuminance(fill, 200, -30), 1);
  // Interpolating encoded channels gives continuous sRGB 0.5, not a byte-rounded stop.
  assert.ok(Math.abs(sampleLuminance(fill, 50, 12) - linearizedEncodedMidpoint) < 1e-14);
  const vertical = { ...fill, start: [5, -10], end: [5, 10] };
  assert.equal(sampleLuminance(vertical, 5, 0), linearizedEncodedMidpoint);
});

test('two-stop radial gradients sample center-to-radius and pad beyond the radius', () => {
  const fill = {
    kind: 'radial-gradient',
    center: [10, -5],
    radius: 20,
    startColor: black,
    endColor: white,
  };
  assert.equal(sampleLuminance(fill, 10, -5), 0);
  assert.equal(sampleLuminance(fill, 30, -5), 1);
  assert.equal(sampleLuminance(fill, 10, 15), 1);
  assert.ok(Math.abs(sampleLuminance(fill, 20, -5) - linearizedEncodedMidpoint) < 1e-14);
});

test('alpha is interpolated with stops and composited over white in linear light', () => {
  const halfTransparentBlack = { hex: '#000000', alpha: 0.5 };
  assert.equal(sampleLuminance(halfTransparentBlack, 0, 0), 0.5);
  assert.equal(sampleLuminance({ hex: '#000000', alpha: 0 }, 0, 0), 1);
  assert.ok(Math.abs(sampleLuminance({ hex: '#ff0000', alpha: 0.5 }, 0, 0) - 0.6063) < 1e-14);
  const fill = {
    kind: 'linear-gradient',
    start: [0, 0],
    end: [10, 0],
    startColor: { hex: '#000000', alpha: 0 },
    endColor: white,
  };
  const midpoint = sampleLuminance(fill, 5, 0),
    expected = 0.5 * linearizedEncodedMidpoint + 0.5;
  assert.ok(Math.abs(midpoint - expected) < 1e-14);
});

test('grid covers intersecting cells and reports world centers, indices and darkness', () => {
  const field = sampleLuminanceGrid(black, bounds);
  assert.equal(field.pitch, 16);
  assert.deepEqual(field.origin, { x: 0, y: 0 });
  assert.deepEqual(field.bounds, bounds);
  assert.deepEqual(field.cells, [
    { i: 0, j: 0, x: 8, y: 8, luminance: 0, darkness: 1 },
    { i: 1, j: 0, x: 24, y: 8, luminance: 0, darkness: 1 },
  ]);
  const shifted = sampleLuminanceGrid(white, {
    minX: -16,
    minY: -8,
    maxX: 0,
    maxY: 0,
  });
  assert.deepEqual(shifted.cells, [{ i: -1, j: -1, x: -8, y: -8, luminance: 1, darkness: 0 }]);
  const inverted = sampleLuminanceGrid(black, bounds, {
    pitch: 8,
    origin: { x: 0, y: 0 },
    invert: true,
  });
  assert.equal(inverted.cells.length, 8);
  assert.ok(inverted.cells.every((cell) => cell.luminance === 0 && cell.darkness === 0));

  // This tiny rectangle lies wholly within one cell and contains no cell center.
  const tiny = sampleLuminanceGrid(black, { minX: 1, minY: 2, maxX: 2, maxY: 3 });
  assert.deepEqual(tiny.cells, [{ i: 0, j: 0, x: 8, y: 8, luminance: 0, darkness: 1 }]);

  // The nonaligned right edge intersects the next cell even though its center is outside.
  const partial = sampleLuminanceGrid(black, { minX: 3, minY: 2, maxX: 17, maxY: 10 });
  assert.deepEqual(
    partial.cells.map(({ i, j, x, y }) => ({ i, j, x, y })),
    [
      { i: 0, j: 0, x: 8, y: 8 },
      { i: 1, j: 0, x: 24, y: 8 },
    ],
  );
});

test('grid handles transparent paint, empty bounds and exactly 4096 cells', () => {
  const transparent = sampleLuminanceGrid({ hex: '#000000', alpha: 0 }, bounds);
  assert.ok(transparent.cells.every((cell) => cell.luminance === 1 && cell.darkness === 0));
  assert.deepEqual(sampleLuminanceGrid(black, { minX: 2, minY: 2, maxX: 2, maxY: 10 }).cells, []);
  assert.equal(
    sampleLuminanceGrid(white, { minX: 0, minY: 0, maxX: 64, maxY: 64 }, { pitch: 1 }).cells.length,
    4096,
  );
});

test('invalid fills, degenerate gradients, points, bounds and grid options are rejected', () => {
  for (const invalidFill of [
    null,
    { hex: 'red', alpha: 1 },
    { hex: '#000000', alpha: 1, extra: true },
    { kind: 'linear-gradient', start: [0, 0], end: [0, 0], startColor: black, endColor: white },
    {
      kind: 'radial-gradient',
      center: [0, 0],
      radius: 0,
      startColor: black,
      endColor: white,
    },
  ])
    assert.throws(() => sampleLuminance(invalidFill, 0, 0));
  for (const [x, y] of [
    [NaN, 0],
    [0, Infinity],
    ['1', 0],
    [null, 0],
  ])
    assert.throws(() => sampleLuminance(black, x, y), /finite/);
  for (const badBounds of [
    null,
    { minX: 0, minY: 0, maxX: -1, maxY: 1 },
    { minX: 0, minY: 0, maxX: Infinity, maxY: 1 },
    { minX: 0, minY: 0, maxX: 1, maxY: 1, extra: true },
  ])
    assert.throws(() => sampleLuminanceGrid(black, badBounds));
  for (const options of [
    null,
    { pitch: 0 },
    { pitch: 1024.1 },
    { pitch: Infinity },
    { origin: { x: NaN, y: 0 } },
    { invert: 1 },
    { extra: true },
  ])
    assert.throws(() => sampleLuminanceGrid(black, bounds, options));
});

test('grid preflights its cell budget and rejects unsafe world indices', () => {
  assert.throws(
    () => sampleLuminanceGrid(black, { minX: 0, minY: 0, maxX: 65, maxY: 65 }, { pitch: 1 }),
    /4096-cell limit/,
  );
  assert.throws(
    () =>
      sampleLuminanceGrid(black, {
        minX: Number.MAX_VALUE / 2,
        minY: 0,
        maxX: Number.MAX_VALUE,
        maxY: 1,
      }),
    /numeric range/,
  );
});
