import test from 'node:test';
import assert from 'node:assert/strict';
import { dashIntervals } from '../src/dash-intervals.js';

const pattern = { dashLength: 3, gapLength: 2, offset: 0 };

test('plans a document-distance dash pattern and clips it to the line', () => {
  assert.deepEqual(dashIntervals(20, pattern), [
    [0, 3],
    [5, 8],
    [10, 13],
    [15, 18],
  ]);
  assert.deepEqual(dashIntervals(9, pattern), [
    [0, 3],
    [5, 8],
  ]);
});

test('positive offset advances the pattern and negative offsets wrap modulo its period', () => {
  assert.deepEqual(dashIntervals(11, { ...pattern, offset: 2 }), [
    [0, 1],
    [3, 6],
    [8, 11],
  ]);
  assert.deepEqual(dashIntervals(10, { ...pattern, offset: -2 }), [
    [2, 5],
    [7, 10],
  ]);
  assert.deepEqual(dashIntervals(8, { ...pattern, offset: 7 }), [
    [0, 1],
    [3, 6],
  ]);
  assert.deepEqual(dashIntervals(8, { ...pattern, offset: -10 }), [
    [0, 3],
    [5, 8],
  ]);
});

test('zero gap is solid regardless of offset, and zero or tiny lengths are safe', () => {
  assert.deepEqual(dashIntervals(7, { dashLength: 1, gapLength: 0, offset: -99999 }), [[0, 7]]);
  assert.deepEqual(dashIntervals(0, pattern), []);
  assert.deepEqual(dashIntervals(0.00001, pattern), [[0, 0.00001]]);
  assert.deepEqual(dashIntervals(0.00001, { ...pattern, offset: 3 }), []);
});

test('accepts exactly the interval budget and rejects a larger pattern', () => {
  assert.equal(dashIntervals(1023, { dashLength: 1, gapLength: 1, offset: 0 }).length, 512);
  assert.throws(
    () => dashIntervals(1025, { dashLength: 1, gapLength: 1, offset: 0 }),
    /512-interval geometry budget/,
  );
  assert.throws(
    () => dashIntervals(Number.MAX_VALUE, { dashLength: 0.0001, gapLength: 0.0001, offset: 0 }),
    /interval count is not finite/,
  );
});

test('rejects invalid lengths and dash settings', () => {
  for (const length of [-1, NaN, Infinity, -Infinity]) {
    assert.throws(() => dashIntervals(length, pattern));
  }
  for (const dashLength of [NaN, Infinity, -Infinity, 0, 0.00009, 100000.01]) {
    assert.throws(() => dashIntervals(10, { ...pattern, dashLength }));
  }
  for (const gapLength of [NaN, Infinity, -Infinity, -0.1, 100000.01]) {
    assert.throws(() => dashIntervals(10, { ...pattern, gapLength }));
  }
  for (const offset of [NaN, Infinity, -Infinity, -100000.01, 100000.01]) {
    assert.throws(() => dashIntervals(10, { ...pattern, offset }));
  }
  assert.throws(() => dashIntervals(10, null));
  assert.throws(() => dashIntervals(10, { dashLength: 1, gapLength: 1 }));
});
