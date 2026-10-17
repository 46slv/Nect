import test from 'node:test';
import assert from 'node:assert/strict';
import { generateLuminanceMarks } from '../src/luminance-marks.js';

const cell = (darkness, overrides = {}) => ({ i: 0, j: 0, x: 12, y: -4, darkness, ...overrides });

function area(vertices) {
  let twiceArea = 0;
  for (let index = 0; index < vertices.length; index++) {
    const [x1, y1] = vertices[index];
    const [x2, y2] = vertices[(index + 1) % vertices.length];
    twiceArea += x1 * y2 - x2 * y1;
  }
  return Math.abs(twiceArea) / 2;
}

function translatedCellArea(mark, pitch) {
  const shifted = mark.vertices.map(([x, y]) => [x - 12, y + 4]);
  return area(shifted) / (pitch * pitch);
}

function ditherGrid(darkness) {
  const cells = [];
  for (let j = 0; j < 8; j++) {
    for (let i = 0; i < 8; i++) cells.push(cell(darkness, { i, j, x: i + 0.5, y: j + 0.5 }));
  }
  return generateLuminanceMarks({ cells, pitch: 1, mode: 'dither' }).marks;
}

test('halftone omits zero darkness and uses an exact square at full darkness', () => {
  const result = generateLuminanceMarks({
    cells: [cell(0), cell(1, { i: 1 })],
    pitch: 4,
    mode: 'halftone',
  });
  assert.equal(result.mode, 'halftone');
  assert.equal(result.pitch, 4);
  assert.equal(result.marks.length, 1);
  assert.deepEqual(result.marks[0], {
    i: 1,
    j: 0,
    vertices: [
      [10, -6],
      [14, -6],
      [14, -2],
      [10, -2],
    ],
  });
});

test('halftone geometry depends on darkness, not any alpha field', () => {
  const lowAlpha = generateLuminanceMarks({
    cells: [cell(0.4, { alpha: 0 })],
    pitch: 8,
    mode: 'halftone',
  });
  const highAlpha = generateLuminanceMarks({
    cells: [cell(0.4, { alpha: 1 })],
    pitch: 8,
    mode: 'halftone',
  });
  assert.deepEqual(lowAlpha, highAlpha);
  assert.equal(lowAlpha.marks[0].vertices.length >= 24, true);
  assert.ok(Math.abs(translatedCellArea(lowAlpha.marks[0], 8) - 0.4) < 1e-8);
});

test('32-step clipped-polygon area calibration is accurate and monotone', () => {
  const pitch = 17.25;
  const darknesses = [0.04, 0.08, 0.15, 0.25, 0.5, 0.72, 0.9, 0.98];
  const fractions = darknesses.map((darkness) => {
    const { marks } = generateLuminanceMarks({ cells: [cell(darkness)], pitch, mode: 'halftone' });
    const fraction = translatedCellArea(marks[0], pitch);
    assert.ok(Math.abs(fraction - darkness) < 1e-8, `${fraction} should approximate ${darkness}`);
    return fraction;
  });
  for (let index = 1; index < fractions.length; index++)
    assert.ok(fractions[index] >= fractions[index - 1]);
});

test('Bayer 8×8 at half tone emits 32 of 64 squares, and full tone emits all 64', () => {
  assert.equal(ditherGrid(0.5).length, 32);
  assert.equal(ditherGrid(1).length, 64);
});

test('dither uses a strict threshold, so a threshold tie does not emit', () => {
  const threshold = 0.5 / 64;
  const tied = generateLuminanceMarks({
    cells: [cell(threshold, { i: 0, j: 0 })],
    pitch: 2,
    mode: 'dither',
  });
  assert.equal(tied.marks.length, 0);
  const above = generateLuminanceMarks({
    cells: [cell(threshold + Number.EPSILON, { i: 0, j: 0 })],
    pitch: 2,
    mode: 'dither',
  });
  assert.equal(above.marks.length, 1);
});

test('dither rank lookup wraps negative world indices with positive modulo', () => {
  const negative = generateLuminanceMarks({
    cells: [cell(0.999, { i: -1, j: 0 })],
    pitch: 3,
    mode: 'dither',
  });
  const wrapped = generateLuminanceMarks({
    cells: [cell(0.999, { i: 7, j: 0 })],
    pitch: 3,
    mode: 'dither',
  });
  assert.deepEqual(negative.marks[0].vertices, wrapped.marks[0].vertices);
  assert.equal(negative.marks.length, 1);
});

test('generation is deterministic and returns geometry in document coordinates', () => {
  const cells = [cell(0.32), cell(1, { i: 1, x: 18 }), cell(0, { i: 2, x: 24 })];
  const first = generateLuminanceMarks({ cells, pitch: 6, mode: 'halftone' });
  const second = generateLuminanceMarks({ cells, pitch: 6, mode: 'halftone' });
  assert.deepEqual(first, second);
  for (const mark of first.marks) {
    for (const [x, y] of mark.vertices) {
      assert.ok(x >= cells.find(({ i }) => i === mark.i).x - 3 - 1e-10);
      assert.ok(x <= cells.find(({ i }) => i === mark.i).x + 3 + 1e-10);
      assert.ok(y >= -7 - 1e-10 && y <= -1 + 1e-10);
    }
  }
});

test('preflights cell, vertex, pitch, and input limits before producing geometry', () => {
  assert.throws(
    () =>
      generateLuminanceMarks({
        cells: Array.from({ length: 4097 }, (_, i) => cell(0, { i })),
        pitch: 1,
        mode: 'dither',
      }),
    /4096 cells/,
  );
  assert.throws(
    () =>
      generateLuminanceMarks({
        cells: Array.from({ length: 3572 }, (_, i) => cell(0.5, { i })),
        pitch: 1,
        mode: 'halftone',
      }),
    /100000 generated vertices/,
  );
  assert.throws(
    () => generateLuminanceMarks({ cells: [cell(0.5)], pitch: 2000, mode: 'halftone' }),
    /0.1 document unit/,
  );
  assert.throws(
    () =>
      generateLuminanceMarks({
        cells: [cell(0.5), { ...cell(0.5), darkness: 1.1 }],
        pitch: 1,
        mode: 'dither',
      }),
    /darkness from 0 to 1/,
  );
});

test('rejects invalid mode, pitch, indices, and coordinates', () => {
  assert.throws(() => generateLuminanceMarks({ cells: [], pitch: 1, mode: 'other' }), /mode/);
  assert.throws(() => generateLuminanceMarks({ cells: [], pitch: 0, mode: 'dither' }), /pitch/);
  assert.throws(
    () => generateLuminanceMarks({ cells: [cell(0.5, { i: 0.5 })], pitch: 1, mode: 'dither' }),
    /integer i\/j/,
  );
  assert.throws(
    () => generateLuminanceMarks({ cells: [cell(0.5, { x: Infinity })], pitch: 1, mode: 'dither' }),
    /integer i\/j/,
  );
});
