import test from 'node:test';
import assert from 'node:assert/strict';
import { taperGeometry } from '../src/taper-geometry.js';

const point = (id, position, handleA = [0, 0], handleB = [0, 0], metadata = {}) => ({
  id,
  position,
  handleA,
  handleB,
  ...metadata,
});

const path = (contours, metadata = {}) => ({ contours, ...metadata });

function closeTo(actual, expected, tolerance = 1e-10) {
  assert.ok(Math.abs(actual - expected) <= tolerance, `${actual} is not close to ${expected}`);
}

function assertIndependentClone(source, result) {
  assert.deepEqual(result, source);
  assert.notEqual(result, source);
  assert.notEqual(result.contours, source.contours);
  for (let contourIndex = 0; contourIndex < source.contours.length; contourIndex++) {
    const sourceContour = source.contours[contourIndex];
    const resultContour = result.contours[contourIndex];
    assert.notEqual(resultContour, sourceContour);
    assert.notEqual(resultContour.points, sourceContour.points);
    for (let pointIndex = 0; pointIndex < sourceContour.points.length; pointIndex++) {
      const sourcePoint = sourceContour.points[pointIndex];
      const resultPoint = resultContour.points[pointIndex];
      assert.notEqual(resultPoint, sourcePoint);
      assert.notEqual(resultPoint.position, sourcePoint.position);
      assert.notEqual(resultPoint.handleA, sourcePoint.handleA);
      assert.notEqual(resultPoint.handleB, sourcePoint.handleB);
    }
  }
}

test('horizontal taper scales Y around the shared bounds midpoint with signed amounts', () => {
  const source = path([
    {
      id: 'outline',
      closed: false,
      points: [point('left', [-1, 0]), point('center', [0, 5]), point('right', [1, 10])],
    },
  ]);

  const positive = taperGeometry(source, { amount: 50, axis: 'horizontal' });
  assert.deepEqual(
    positive.contours[0].points.map(({ position }) => position),
    [
      [-1, 2.5],
      [0, 5],
      [1, 12.5],
    ],
  );

  const negative = taperGeometry(source, { amount: -50, axis: 'horizontal' });
  assert.deepEqual(
    negative.contours[0].points.map(({ position }) => position),
    [
      [-1, -2.5],
      [0, 5],
      [1, 7.5],
    ],
  );
});

test('vertical taper swaps the deform and scaled axes', () => {
  const source = path([
    {
      id: 'vertical',
      closed: true,
      points: [point('bottom', [0, -1]), point('middle', [5, 0]), point('top', [10, 1])],
    },
  ]);
  const result = taperGeometry(source, { amount: 50, axis: 'vertical' });
  assert.deepEqual(
    result.contours[0].points.map(({ position }) => position),
    [
      [2.5, -1],
      [5, 0],
      [12.5, 1],
    ],
  );
});

test('maps absolute control endpoints and recomputes their relative handles', () => {
  const source = path([
    {
      id: 'curve',
      closed: false,
      points: [point('start', [0, 0], [0, 0], [2, 4]), point('end', [4, 8], [-2, -4], [0, 0])],
    },
  ]);
  const result = taperGeometry(source, { amount: 50, axis: 'horizontal' });
  const [start, end] = result.contours[0].points;

  assert.deepEqual(start.position, [0, 2]);
  assert.deepEqual(start.handleB, [2, 2]);
  assert.deepEqual(end.position, [4, 10]);
  assert.deepEqual(end.handleA, [-2, -6]);
  assert.notDeepEqual(start.handleB, source.contours[0].points[0].handleB);
  assert.notDeepEqual(end.handleA, source.contours[0].points[1].handleA);
});

test('absolute handle endpoints contribute to the bounds used for every point', () => {
  const source = path([
    {
      id: 'curve',
      closed: false,
      points: [point('start', [1, 0], [0, 0], [3, 0]), point('end', [3, 10])],
    },
  ]);
  const result = taperGeometry(source, { amount: 50, axis: 'horizontal' });

  // The first point's outgoing absolute control endpoint at X=4 extends the
  // horizontal bounds beyond the second anchor at X=3.
  assert.deepEqual(result.contours[0].points[0].position, [1, 2.5]);
  closeTo(result.contours[0].points[1].position[1], 10 + 5 / 6);
  assert.deepEqual(result.contours[0].points[0].handleB, [3, -5]);
});

test('all contours share one bounds frame and preserve order, groups, flags, and metadata', () => {
  const source = path(
    [
      {
        id: 'first',
        group: 'glyph-a',
        closed: false,
        metadata: { role: 'stem' },
        points: [point('a', [2, 0]), point('b', [4, 10])],
      },
      {
        id: 'second',
        group: 'glyph-b',
        closed: true,
        points: [point('c', [0, 0])],
      },
    ],
    { name: 'multi-glyph', metadata: { source: 'fixture' } },
  );
  const before = structuredClone(source);
  const result = taperGeometry(source, { amount: 50, axis: 'horizontal', prefix: 'fixture-taper' });

  assert.deepEqual(source, before);
  assert.deepEqual(result.metadata, source.metadata);
  assert.deepEqual(
    result.contours.map(({ group, closed, metadata }) => ({ group, closed, metadata })),
    source.contours.map(({ group, closed, metadata }) => ({ group, closed, metadata })),
  );
  assert.deepEqual(
    result.contours.map(({ id }) => id),
    ['fixture-taper:c0', 'fixture-taper:c1'],
  );
  assert.deepEqual(
    result.contours.map(({ points }) => points.map(({ id }) => id)),
    [['fixture-taper:c0:p0', 'fixture-taper:c0:p1'], ['fixture-taper:c1:p0']],
  );
  // The first contour alone would have X bounds [2, 4]. The other contour's
  // X=0 anchor extends the real whole-input bounds, making X=2 the midpoint.
  assert.deepEqual(result.contours[0].points[0].position, [2, 0]);
  assert.deepEqual(result.contours[1].points[0].position, [0, 2.5]);
  assert.deepEqual(
    result,
    taperGeometry(source, { amount: 50, axis: 'horizontal', prefix: 'fixture-taper' }),
  );
});

test('empty input, zero amount, and zero deform-axis span each return independent clones', () => {
  const empty = path([], { metadata: { keep: true } });
  assertIndependentClone(empty, taperGeometry(empty, { amount: 20 }));

  const spread = path([
    { id: 'spread', closed: false, points: [point('a', [0, 1], [1, 2]), point('b', [5, 3])] },
  ]);
  const zeroAmount = taperGeometry(spread, { amount: 0, axis: 'horizontal' });
  assertIndependentClone(spread, zeroAmount);

  const verticalLine = path([
    { id: 'line', closed: false, points: [point('a', [2, 0]), point('b', [2, 5])] },
  ]);
  const zeroSpan = taperGeometry(verticalLine, { amount: 50, axis: 'horizontal' });
  assertIndependentClone(verticalLine, zeroSpan);
});

test('defaults to zero horizontal taper and validates options and prefixes', () => {
  const source = path([{ id: 'single', closed: true, points: [point('p', [3, 4])] }]);
  assertIndependentClone(source, taperGeometry(source));
  assertIndependentClone(source, taperGeometry(source, { amount: undefined, axis: undefined }));
  assert.doesNotThrow(() => taperGeometry(source, { amount: -50, axis: 'vertical' }));
  assert.doesNotThrow(() => taperGeometry(source, { amount: 50, axis: 'horizontal' }));

  for (const amount of [-50.01, 50.01, NaN, Infinity, -Infinity])
    assert.throws(() => taperGeometry(source, { amount }), /amount/i);
  for (const axis of ['diagonal', '', null])
    assert.throws(() => taperGeometry(source, { amount: 1, axis }), /axis/i);
  assert.throws(() => taperGeometry(source, null), /options/i);
  assert.throws(() => taperGeometry(source, []), /options/i);
  assert.throws(() => taperGeometry(source, { prefix: '' }), /prefix/i);
  assert.throws(() => taperGeometry(source, { prefix: 4 }), /prefix/i);
});

test('changed output is immutable, deterministic, and finite', () => {
  const source = path(
    [
      {
        id: 'open',
        group: 'g1',
        closed: false,
        points: [point('a', [-1e12, 100], [0, 1], [2, 3]), point('b', [1e12, -100])],
      },
    ],
    { label: 'source', metadata: { tags: ['fixture'] } },
  );
  const before = structuredClone(source);
  const options = { amount: 50, axis: 'horizontal', prefix: 'deterministic' };
  const first = taperGeometry(source, options);

  assert.deepEqual(first, taperGeometry(source, options));
  assert.deepEqual(source, before);
  for (const contour of first.contours) {
    for (const item of contour.points) {
      assert.ok(item.position.every(Number.isFinite));
      assert.ok(item.handleA.every(Number.isFinite));
      assert.ok(item.handleB.every(Number.isFinite));
    }
  }
  first.contours[0].points[0].position[0] = 999;
  assert.deepEqual(source, before);
});

test('rejects malformed source geometry, budget excess, and coordinate overflow', () => {
  assert.throws(() => taperGeometry(null), /Path input/i);
  assert.throws(() => taperGeometry({ contours: {} }), /Path input/i);
  assert.throws(
    () => taperGeometry({ contours: [{ id: '', closed: false, points: [] }] }),
    /contours/i,
  );
  assert.throws(
    () => taperGeometry({ contours: [{ id: 'bad', closed: false, points: null }] }),
    /points/i,
  );
  assert.throws(
    () => taperGeometry(path([{ id: 'bad', closed: true, points: [point('p', [NaN, 0])] }])),
    /finite positions and handles/i,
  );
  assert.throws(
    () =>
      taperGeometry(
        path([
          {
            id: 'overflowing-handle',
            closed: false,
            points: [point('p', [Number.MAX_VALUE, 0], [Number.MAX_VALUE, 0])],
          },
        ]),
      ),
    /finite-coordinate arithmetic/i,
  );
  assert.throws(() => taperGeometry(path(new Array(10001).fill({}))), /geometry budget/i);
  assert.throws(
    () =>
      taperGeometry(
        path([
          {
            id: 'large',
            closed: false,
            points: new Array(100001).fill(point('p', [0, 0])),
          },
        ]),
      ),
    /geometry budget/i,
  );
});

test('rejects transformed coordinates or recomputed relative handles that overflow', () => {
  const source = path([
    {
      id: 'wide',
      closed: false,
      points: [point('bottom', [0, -Number.MAX_VALUE]), point('top', [1, Number.MAX_VALUE])],
    },
  ]);
  assert.throws(
    () => taperGeometry(source, { amount: 50, axis: 'horizontal' }),
    /finite-coordinate arithmetic/i,
  );
});
