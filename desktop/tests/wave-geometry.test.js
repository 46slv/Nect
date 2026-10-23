import test from 'node:test';
import assert from 'node:assert/strict';
import { waveGeometry } from '../src/wave-geometry.js';

const point = (id, position, handleA = [0, 0], handleB = [0, 0]) => ({
  id,
  position,
  handleA,
  handleB,
});

function closeTo(actual, expected, tolerance = 1e-10) {
  assert.ok(Math.abs(actual - expected) <= tolerance, `${actual} is not close to ${expected}`);
}

function line(points, { closed = false, group = 'g', id = 'line' } = {}) {
  return { contours: [{ id, closed, group, points }] };
}

test('vertical and horizontal directions displace anchors by a quarter-period amount', () => {
  const vertical = waveGeometry(line([point('a', [2.5, 3])]), {
    amplitude: 8,
    wavelength: 10,
    phase: 0,
    direction: 'vertical',
  });
  const horizontal = waveGeometry(line([point('a', [3, 2.5])]), {
    amplitude: 8,
    wavelength: 10,
    phase: 0,
    direction: 'horizontal',
  });
  assert.deepEqual(vertical.contours[0].points[0].position, [2.5, 11]);
  assert.deepEqual(horizontal.contours[0].points[0].position, [11, 2.5]);
});

test('negative amplitude reverses the displacement', () => {
  const result = waveGeometry(line([point('a', [2.5, 3])]), {
    amplitude: -8,
    wavelength: 10,
    phase: 0,
    direction: 'vertical',
  });
  assert.deepEqual(result.contours[0].points[0].position, [2.5, -5]);
});

test('phase endpoints are equivalent to zero and phase is periodic', () => {
  const source = line([point('a', [1.25, 2])]);
  const evaluate = (phase) =>
    waveGeometry(source, { amplitude: 4, wavelength: 5, phase, direction: 'vertical' });
  assert.deepEqual(evaluate(-360), evaluate(0));
  assert.deepEqual(evaluate(360), evaluate(0));
  closeTo(
    evaluate(37).contours[0].points[0].position[1],
    evaluate(-323).contours[0].points[0].position[1],
  );
  const shifted = waveGeometry(line([point('a', [6.25, 2])]), {
    amplitude: 4,
    wavelength: 5,
    phase: 0,
    direction: 'vertical',
  });
  closeTo(evaluate(0).contours[0].points[0].position[1], shifted.contours[0].points[0].position[1]);
});

test('keeps open endpoints fixed only when requested and never pins closed contours', () => {
  const open = line([point('first', [0, 1]), point('middle', [2.5, 2]), point('last', [7.5, 3])]);
  const pinned = waveGeometry(open, {
    amplitude: 6,
    wavelength: 10,
    phase: 0,
    direction: 'vertical',
    keepEndpoints: true,
  });
  const free = waveGeometry(open, {
    amplitude: 6,
    wavelength: 10,
    phase: 0,
    direction: 'vertical',
  });
  assert.deepEqual(
    pinned.contours[0].points.map(({ position }) => position),
    [
      [0, 1],
      [2.5, 8],
      [7.5, 3],
    ],
  );
  assert.deepEqual(
    free.contours[0].points.map(({ position }) => position),
    [
      [0, 1],
      [2.5, 8],
      [7.5, -3],
    ],
  );

  const closed = line([point('first', [0, 1]), point('last', [7.5, 3])], { closed: true });
  const closedResult = waveGeometry(closed, {
    amplitude: 6,
    wavelength: 10,
    phase: 0,
    direction: 'vertical',
    keepEndpoints: true,
  });
  assert.deepEqual(
    closedResult.contours[0].points.map(({ position }) => position),
    [
      [0, 1],
      [7.5, -3],
    ],
  );
});

test('preserves groups, contour order, closed flags, relative handles, and namespaced IDs', () => {
  const source = {
    label: 'source',
    contours: [
      {
        id: 'open',
        group: 'g1',
        closed: false,
        points: [point('a', [2.5, 3], [-1, 2], [4, -5])],
      },
      { id: 'closed', group: 'g2', closed: true, points: [point('b', [7.5, 9])] },
    ],
  };
  const result = waveGeometry(source, {
    amplitude: 2,
    wavelength: 10,
    phase: 0,
    direction: 'vertical',
    prefix: 'fixture-wave',
  });
  assert.deepEqual(
    result.contours.map(({ group, closed }) => ({ group, closed })),
    [
      { group: 'g1', closed: false },
      { group: 'g2', closed: true },
    ],
  );
  assert.deepEqual(
    result.contours.map(({ id }) => id),
    ['fixture-wave:c0', 'fixture-wave:c1'],
  );
  assert.deepEqual(
    result.contours.map(({ points }) => points.map(({ id }) => id)),
    [['fixture-wave:c0:p0'], ['fixture-wave:c1:p0']],
  );
  assert.deepEqual(result.contours[0].points[0].handleA, [-1, 2]);
  assert.deepEqual(result.contours[0].points[0].handleB, [4, -5]);
  assert.deepEqual(result.contours[0].points[0].position, [2.5, 5]);
});

test('amplitude zero returns an independent geometry-equivalent clone', () => {
  const source = line([point('p', [2, 3], [4, 5], [-6, 7])]);
  const before = structuredClone(source);
  const result = waveGeometry(source, {
    amplitude: 0,
    wavelength: 10,
    phase: 0,
    direction: 'vertical',
  });
  assert.deepEqual(result, source);
  assert.notEqual(result, source);
  assert.notEqual(result.contours, source.contours);
  assert.notEqual(result.contours[0], source.contours[0]);
  assert.notEqual(result.contours[0].points[0].position, source.contours[0].points[0].position);
  result.contours[0].points[0].handleA[0] = 999;
  assert.deepEqual(source, before);
});

test('is immutable and deterministic with bounded finite output', () => {
  const source = line([point('p0', [-1e12, 100]), point('p1', [1e12, -100])]);
  const before = structuredClone(source);
  const options = {
    amplitude: 100000,
    wavelength: 1,
    phase: -360,
    direction: 'vertical',
    prefix: 'deterministic',
  };
  const first = waveGeometry(source, options);
  const second = waveGeometry(source, options);
  assert.deepEqual(first, second);
  assert.deepEqual(source, before);
  for (const contour of first.contours) {
    for (const item of contour.points) {
      assert.ok(item.position.every(Number.isFinite));
      assert.ok(item.handleA.every(Number.isFinite));
      assert.ok(item.handleB.every(Number.isFinite));
    }
  }
});

test('rejects invalid option ranges, directions, endpoint flag, and ID prefixes', () => {
  const source = line([point('p', [1, 2])]);
  assert.doesNotThrow(() =>
    waveGeometry(line([point('p', [0, 0])]), {
      amplitude: -100000,
      wavelength: 0.0001,
      phase: 360,
      direction: 'horizontal',
    }),
  );
  assert.doesNotThrow(() =>
    waveGeometry(line([point('p', [0, 0])]), {
      amplitude: 100000,
      wavelength: 100000,
      phase: -360,
      direction: 'vertical',
    }),
  );
  for (const amplitude of [-100001, 100001, NaN, Infinity, undefined])
    assert.throws(
      () => waveGeometry(source, { amplitude, wavelength: 1, phase: 0, direction: 'vertical' }),
      /amplitude/i,
    );
  for (const wavelength of [0, 0.00009, 100001, NaN, Infinity, undefined])
    assert.throws(
      () => waveGeometry(source, { amplitude: 1, wavelength, phase: 0, direction: 'vertical' }),
      /wavelength/i,
    );
  for (const phase of [-361, 361, NaN, Infinity, undefined])
    assert.throws(
      () => waveGeometry(source, { amplitude: 1, wavelength: 1, phase, direction: 'vertical' }),
      /phase/i,
    );
  for (const direction of ['diagonal', '', null, undefined])
    assert.throws(
      () => waveGeometry(source, { amplitude: 1, wavelength: 1, phase: 0, direction }),
      /direction/i,
    );
  assert.throws(
    () =>
      waveGeometry(source, {
        amplitude: 1,
        wavelength: 1,
        phase: 0,
        direction: 'vertical',
        keepEndpoints: 'yes',
      }),
    /keepEndpoints/i,
  );
  assert.throws(
    () =>
      waveGeometry(source, {
        amplitude: 1,
        wavelength: 1,
        phase: 0,
        direction: 'vertical',
        prefix: '',
      }),
    /prefix/i,
  );
});

test('rejects invalid sources and geometry-budget excess', () => {
  const options = { amplitude: 1, wavelength: 1, phase: 0, direction: 'vertical' };
  assert.throws(() => waveGeometry(null, options), /Path input/i);
  assert.throws(() => waveGeometry({ contours: [{ points: [] }] }, options), /contours/i);
  assert.throws(
    () => waveGeometry({ contours: new Array(10001).fill({}) }, options),
    /geometry budget/i,
  );
  assert.throws(
    () => waveGeometry(line(new Array(100001).fill(point('p', [0, 0]))), options),
    /geometry budget/i,
  );
  assert.throws(
    () => waveGeometry(line([point('p', [NaN, 0])]), options),
    /finite positions and handles/i,
  );
  assert.throws(
    () => waveGeometry(line([point('p', [0, 0], [Infinity, 0])]), options),
    /finite positions and handles/i,
  );
});

test('rejects coordinate-to-wavelength ratios beyond the documented precision limit', () => {
  assert.throws(
    () =>
      waveGeometry(line([point('p', [1.0001e12, 0])]), {
        amplitude: 1,
        wavelength: 1,
        phase: 0,
        direction: 'vertical',
      }),
    /coordinate precision limit/i,
  );
});
