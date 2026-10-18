import test from 'node:test';
import assert from 'node:assert/strict';
import { autoHandlesGeometry } from '../src/auto-handles-geometry.js';

const point = (id, position, handleA = [0, 0], handleB = [0, 0]) => ({
  id,
  position,
  handleA,
  handleB,
});

function path(positions, closed = false) {
  return {
    contours: [
      {
        id: 'source-contour',
        closed,
        points: positions.map((position, index) => point(`source-${index}`, position)),
      },
    ],
  };
}

function handles(geometry) {
  return geometry.contours[0].points.map(({ handleA, handleB }) => ({ handleA, handleB }));
}

function assertVectorClose(actual, expected, epsilon = 1e-12) {
  assert.equal(actual.length, expected.length);
  actual.forEach((value, index) => assert.ok(Math.abs(value - expected[index]) <= epsilon));
}

test('uses chord-scaled tangent handles and the one-third endpoint rule', () => {
  const result = autoHandlesGeometry(
    path([
      [0, 0],
      [3, 0],
      [3, 4],
    ]),
  );
  assert.deepEqual(handles(result), [
    { handleA: [0, 0], handleB: [1, 0] },
    { handleA: [-0.6, -0.8], handleB: [0.6, 0.8] },
    { handleA: [0, -4 / 3], handleB: [0, 0] },
  ]);
});

test('uses the shorter adjacent chord and makes interior handles symmetric', () => {
  const result = autoHandlesGeometry(
    path([
      [0, 0],
      [2, 0],
      [8, 0],
    ]),
  );
  assert.deepEqual(handles(result), [
    { handleA: [0, 0], handleB: [2 / 3, 0] },
    { handleA: [-2 / 3, 0], handleB: [2 / 3, 0] },
    { handleA: [-2, 0], handleB: [0, 0] },
  ]);
  const middle = result.contours[0].points[1];
  assert.deepEqual(
    middle.handleA.map((value) => (value === 0 ? 0 : -value)),
    middle.handleB,
  );
  assert.ok(Math.hypot(...middle.handleB) <= 2 / 3);
});

test('wraps closed-contour tangent neighbors across the seam without duplicating anchors', () => {
  const result = autoHandlesGeometry(
    path(
      [
        [0, 0],
        [2, 0],
        [2, 2],
        [0, 2],
      ],
      true,
    ),
  );
  assert.equal(result.contours[0].closed, true);
  assert.equal(result.contours[0].points.length, 4);
  const component = Math.SQRT1_2 * (2 / 3);
  assertVectorClose(result.contours[0].points[0].handleB, [component, -component]);
  assertVectorClose(result.contours[0].points[0].handleA, [-component, component]);
});

test('zeros both paired handles for duplicate adjacent anchors or a zero tangent', () => {
  const duplicate = autoHandlesGeometry(
    path([
      [0, 0],
      [0, 0],
      [3, 0],
    ]),
  );
  assert.deepEqual(handles(duplicate), [
    { handleA: [0, 0], handleB: [0, 0] },
    { handleA: [0, 0], handleB: [0, 0] },
    { handleA: [-1, 0], handleB: [0, 0] },
  ]);

  const reversal = autoHandlesGeometry(
    path([
      [0, 0],
      [2, 0],
      [0, 0],
    ]),
  );
  assert.deepEqual(reversal.contours[0].points[1].handleA, [0, 0]);
  assert.deepEqual(reversal.contours[0].points[1].handleB, [0, 0]);
});

test('handles empty, singleton, two-point open, and two-point closed contours', () => {
  const source = {
    contours: [
      { id: 'empty', closed: false, points: [] },
      { id: 'single-open', closed: false, points: [point('s', [7, -3], [4, 5], [2, 1])] },
      { id: 'two-open', closed: false, points: [point('a', [0, 0]), point('b', [6, 3])] },
      { id: 'two-closed', closed: true, points: [point('c', [0, 0]), point('d', [6, 3])] },
    ],
  };
  const result = autoHandlesGeometry(source);
  assert.deepEqual(result.contours[0].points, []);
  assert.deepEqual(handles({ contours: [result.contours[1]] }), [
    { handleA: [0, 0], handleB: [0, 0] },
  ]);
  assert.deepEqual(handles({ contours: [result.contours[2]] }), [
    { handleA: [0, 0], handleB: [2, 1] },
    { handleA: [-2, -1], handleB: [0, 0] },
  ]);
  assert.deepEqual(handles({ contours: [result.contours[3]] }), [
    { handleA: [0, 0], handleB: [0, 0] },
    { handleA: [0, 0], handleB: [0, 0] },
  ]);
});

test('strength zero clears every handle, validates options, and namespaces stable IDs', () => {
  const source = path([
    [0, 0],
    [3, 0],
    [6, 0],
  ]);
  source.contours[0].points[1].handleA = [-9, 4];
  source.contours[0].points[1].handleB = [2, 7];
  const result = autoHandlesGeometry(source, { strength: 0, prefix: 'modifier-7' });
  assert.deepEqual(handles(result), [
    { handleA: [0, 0], handleB: [0, 0] },
    { handleA: [0, 0], handleB: [0, 0] },
    { handleA: [0, 0], handleB: [0, 0] },
  ]);
  assert.equal(result.contours[0].id, 'modifier-7:c0');
  assert.deepEqual(
    result.contours[0].points.map(({ id }) => id),
    ['modifier-7:c0:p0', 'modifier-7:c0:p1', 'modifier-7:c0:p2'],
  );

  for (const strength of [-0.01, 1.01, Infinity, NaN])
    assert.throws(() => autoHandlesGeometry(source, { strength }), /strength/i);
  assert.throws(() => autoHandlesGeometry(source, null), /options/i);
  assert.throws(() => autoHandlesGeometry(source, { prefix: '' }), /prefix/i);
  assert.throws(() => autoHandlesGeometry(source, { prefix: 3 }), /prefix/i);
});

test('preserves contour groups and metadata, keeps anchors unchanged, and clones independently', () => {
  const source = {
    name: 'authored path',
    metadata: { source: 'test', nested: { labels: ['keep'] } },
    contours: [
      {
        id: 'grouped',
        closed: true,
        group: 'outer',
        metadata: { role: 'outline', custom: { weight: 2 } },
        points: [
          { ...point('left', [0, 0], [-1, 0], [1, 0]), tag: { name: 'left node' } },
          point('right', [4, 0], [-1, 0], [1, 0]),
          point('top', [4, 4], [-1, 0], [1, 0]),
        ],
      },
    ],
  };
  const before = structuredClone(source);
  const result = autoHandlesGeometry(source);

  assert.deepEqual(source, before);
  assert.deepEqual(result.name, source.name);
  assert.deepEqual(result.metadata, source.metadata);
  assert.deepEqual(result.contours[0].group, 'outer');
  assert.deepEqual(result.contours[0].metadata, source.contours[0].metadata);
  assert.deepEqual(
    result.contours[0].points.map(({ position }) => position),
    source.contours[0].points.map(({ position }) => position),
  );
  assert.deepEqual(
    result.contours[0].points.map(({ tag }) => tag),
    [{ name: 'left node' }, undefined, undefined],
  );
  assert.notEqual(result.metadata, source.metadata);
  assert.notEqual(result.metadata.nested, source.metadata.nested);
  assert.notEqual(result.contours[0].metadata.custom, source.contours[0].metadata.custom);
  assert.notEqual(result.contours[0].points[0].tag, source.contours[0].points[0].tag);

  result.metadata.nested.labels.push('result only');
  result.contours[0].metadata.custom.weight = 9;
  result.contours[0].points[0].position[0] = 99;
  assert.deepEqual(source, before);
});

test('rejects malformed geometry, non-finite inputs, and contour or point budget overflow', () => {
  const source = path([
    [0, 0],
    [1, 1],
  ]);
  assert.throws(() => autoHandlesGeometry(null), /Path input/i);
  assert.throws(() => autoHandlesGeometry({ contours: [{}] }), /contours/i);

  const invalid = structuredClone(source);
  invalid.contours[0].points[1].handleB[0] = Infinity;
  assert.throws(() => autoHandlesGeometry(invalid), /finite/i);

  assert.throws(
    () => autoHandlesGeometry({ contours: Array(10001).fill(source.contours[0]) }),
    /budget/i,
  );
  assert.throws(
    () =>
      autoHandlesGeometry({
        contours: [{ id: 'too-many', closed: false, points: Array(100001) }],
      }),
    /budget/i,
  );
});

test('uses safe arithmetic for extreme finite coordinates and rejects overflowing endpoints', () => {
  const max = Number.MAX_VALUE;
  const extreme = autoHandlesGeometry(
    path([
      [max, 0],
      [-max, 0],
    ]),
  );
  const handleLength = max * (2 / 3);
  assert.deepEqual(extreme.contours[0].points[0].handleB, [-handleLength, 0]);
  assert.deepEqual(extreme.contours[0].points[1].handleA, [handleLength, 0]);
  assert.ok(
    extreme.contours[0].points.every((item) =>
      [...item.handleA, ...item.handleB, ...item.position].every(Number.isFinite),
    ),
  );

  const overflowingAbsoluteEndpoint = path([
    [-max, 0],
    [max, 0],
    [max, 1e308],
  ]);
  assert.throws(
    () => autoHandlesGeometry(overflowingAbsoluteEndpoint),
    /absolute handle endpoints/i,
  );
});
