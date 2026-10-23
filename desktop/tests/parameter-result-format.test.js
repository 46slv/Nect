import test from 'node:test';
import assert from 'node:assert/strict';
import { formatParameterResult } from '../src/parameter-result-format.js';

test('tiny positive and negative results remain nonzero, including vector components', () => {
  assert.equal(formatParameterResult(0.0001), '0.0001');
  assert.equal(formatParameterResult(-0.0001), '-0.0001');
  assert.equal(formatParameterResult(0.000123456), '0.000123');
  assert.equal(formatParameterResult(-0.000123456), '-0.000123');
  assert.equal([0.0001, -0.0001].map(formatParameterResult).join(', '), '0.0001, -0.0001');
  for (const magnitude of [Number.MIN_VALUE, 1e-100, Number.EPSILON, 0.000499999]) {
    for (const value of [magnitude, -magnitude]) {
      const displayed = Number(formatParameterResult(value));
      assert.ok(Number.isFinite(displayed));
      assert.notEqual(displayed, 0);
      assert.equal(Math.sign(displayed), Math.sign(value));
    }
  }
});

test('zero, ordinary values and rounding boundaries keep the previous compact presentation', () => {
  assert.equal(formatParameterResult(0), '0');
  assert.equal(formatParameterResult(-0), '0');
  assert.equal(formatParameterResult(12.34567), '12.346');
  assert.equal(formatParameterResult(-12.34567), '-12.346');
  assert.equal(formatParameterResult(100), '100');
  assert.equal(formatParameterResult(0.000499999), '0.0005');
  assert.equal(formatParameterResult(-0.000499999), '-0.0005');
  for (const value of [
    0.0005,
    -0.0005,
    0.000500001,
    -0.000500001,
    1.000499999,
    1.000500001,
    1e20,
    Number.MAX_VALUE,
  ])
    assert.equal(formatParameterResult(value), String(Number(value.toFixed(3))));
});
