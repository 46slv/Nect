import test from 'node:test';
import assert from 'node:assert/strict';
import { reversePathGeometry } from '../src/reverse-path-geometry.js';

const point = (id, position, handleA = [0, 0], handleB = [0, 0], metadata = {}) => ({
  id,
  position,
  handleA,
  handleB,
  ...metadata,
});

const sourcePath = () => ({
  label: 'source path',
  metadata: { authoring: ['pen', 'imported'] },
  contours: [
    {
      id: 'open',
      closed: false,
      group: 'shape-a',
      metadata: { role: 'outline' },
      points: [
        point('open-a', [0, 0], [-2, 1], [3, 4], { label: 'start' }),
        point('open-b', [20, 10], [-1, 5], [6, -3], { label: 'middle' }),
        point('open-c', [40, 0], [2, -1], [4, 2], { label: 'end' }),
      ],
    },
    {
      id: 'closed',
      closed: true,
      group: 'shape-b',
      metadata: { role: 'hole' },
      points: [
        point('closed-a', [5, 5], [-1, -2], [3, 1]),
        point('closed-b', [15, 5], [-2, 3], [1, 4]),
        point('closed-c', [15, 15], [-3, 1], [2, -4]),
        point('closed-d', [5, 15], [4, 2], [-1, -3]),
      ],
    },
  ],
});

function cubicSegments(contour) {
  const segments = [];
  const segmentCount = contour.closed ? contour.points.length : contour.points.length - 1;
  for (let index = 0; index < segmentCount; index++) {
    const start = contour.points[index];
    const end = contour.points[(index + 1) % contour.points.length];
    segments.push([
      [...start.position],
      start.position.map((value, axis) => value + start.handleB[axis]),
      end.position.map((value, axis) => value + end.handleA[axis]),
      [...end.position],
    ]);
  }
  return segments;
}

test('reverses open cubic segments exactly and reverses all point order', () => {
  const source = sourcePath();
  const reversed = reversePathGeometry(source, { prefix: 'fixture-reverse' }).contours[0];

  assert.deepEqual(
    reversed.points.map(({ id }) => id),
    ['fixture-reverse:c0:p0', 'fixture-reverse:c0:p1', 'fixture-reverse:c0:p2'],
  );
  assert.deepEqual(
    reversed.points.map(({ label }) => label),
    ['end', 'middle', 'start'],
  );
  const expectedSegments = cubicSegments(source.contours[0])
    .reverse()
    .map((segment) => [...segment].reverse());
  assert.deepEqual(cubicSegments(reversed), expectedSegments);
});

test('reverses closed cubic geometry while preserving its stored anchor seam', () => {
  const source = sourcePath();
  const original = source.contours[1];
  const reversed = reversePathGeometry(source).contours[1];

  assert.deepEqual(reversed.points[0].position, original.points[0].position);
  assert.deepEqual(
    reversed.points.map((item) => item.position),
    [
      original.points[0].position,
      ...original.points
        .slice(1)
        .reverse()
        .map((item) => item.position),
    ],
  );
  const expectedSegments = cubicSegments(original)
    .reverse()
    .map((segment) => [...segment].reverse());
  assert.deepEqual(cubicSegments(reversed), expectedSegments);
});

test('handles zero, one, and two point open and closed contours', () => {
  const fixtures = [
    { id: 'open-zero', closed: false, points: [] },
    { id: 'open-one', closed: false, points: [point('open-one-p', [1, 2], [3, 4], [5, 6])] },
    {
      id: 'open-two',
      closed: false,
      points: [point('open-two-a', [1, 2]), point('open-two-b', [3, 4])],
    },
    { id: 'closed-zero', closed: true, points: [] },
    { id: 'closed-one', closed: true, points: [point('closed-one-p', [5, 6], [7, 8], [9, 10])] },
    {
      id: 'closed-two',
      closed: true,
      points: [
        point('closed-two-a', [11, 12], [13, 14], [15, 16]),
        point('closed-two-b', [17, 18]),
      ],
    },
  ];
  const result = reversePathGeometry({ contours: fixtures });

  assert.deepEqual(
    result.contours.map(({ points }) => points.length),
    [0, 1, 2, 0, 1, 2],
  );
  assert.deepEqual(result.contours[1].points[0].handleA, [5, 6]);
  assert.deepEqual(result.contours[1].points[0].handleB, [3, 4]);
  assert.deepEqual(
    result.contours[2].points.map(({ position }) => position),
    [
      [3, 4],
      [1, 2],
    ],
  );
  assert.deepEqual(result.contours[4].points[0].handleA, [9, 10]);
  assert.deepEqual(result.contours[4].points[0].handleB, [7, 8]);
  assert.deepEqual(
    result.contours[5].points.map(({ position }) => position),
    [
      [11, 12],
      [17, 18],
    ],
  );
  assert.deepEqual(result.contours[5].points[0].handleA, [15, 16]);
});

test('preserves groups and metadata, creates deterministic IDs, and leaves source untouched', () => {
  const source = sourcePath();
  const before = structuredClone(source);
  const options = { prefix: 'named' };
  const result = reversePathGeometry(source, options);

  assert.deepEqual(result, reversePathGeometry(source, options));
  assert.deepEqual(source, before);
  assert.deepEqual(result.metadata, source.metadata);
  assert.deepEqual(
    result.contours.map(({ group, metadata }) => ({ group, metadata })),
    source.contours.map(({ group, metadata }) => ({ group, metadata })),
  );
  assert.equal(result.contours[0].points[0].label, 'end');
  assert.equal(result.contours[0].points[0].id, 'named:c0:p0');
  assert.equal(result.contours[1].points[0].id, 'named:c1:p0');
  const contourIds = result.contours.map(({ id }) => id);
  const pointIds = result.contours.flatMap((contour) => contour.points.map(({ id }) => id));
  assert.deepEqual(contourIds, ['named:c0', 'named:c1']);
  assert.equal(new Set(contourIds).size, contourIds.length);
  assert.equal(new Set(pointIds).size, pointIds.length);

  assert.notEqual(result, source);
  assert.notEqual(result.contours, source.contours);
  assert.notEqual(result.contours[0], source.contours[0]);
  assert.notEqual(result.contours[0].points, source.contours[0].points);
  assert.notEqual(result.contours[0].points[0].position, source.contours[0].points[2].position);
  assert.notEqual(result.contours[0].points[0].handleA, source.contours[0].points[2].handleB);
  result.contours[0].points[0].position[0] = 900;
  result.contours[0].points[0].handleA[0] = 900;
  assert.deepEqual(source, before);
});

test('rejects malformed options, invalid geometry, non-finite vectors, and geometry budgets', () => {
  const source = sourcePath();
  assert.throws(() => reversePathGeometry(source, null), /options/i);
  assert.throws(() => reversePathGeometry(source, []), /options/i);
  assert.throws(() => reversePathGeometry(source, { prefix: '' }), /prefix/i);
  assert.throws(() => reversePathGeometry(source, { prefix: 4 }), /prefix/i);
  assert.throws(() => reversePathGeometry(null), /Path input/i);
  assert.throws(() => reversePathGeometry({ contours: {} }), /Path input/i);
  assert.throws(
    () => reversePathGeometry({ contours: [{ id: '', closed: false, points: [] }] }),
    /contours/i,
  );
  assert.throws(
    () => reversePathGeometry({ contours: [{ id: 'bad', closed: false, points: null }] }),
    /points/i,
  );

  const invalidPoint = point('bad', [1, 2]);
  invalidPoint.handleB[0] = Infinity;
  assert.throws(
    () => reversePathGeometry({ contours: [{ id: 'bad', closed: true, points: [invalidPoint] }] }),
    /finite/i,
  );
  assert.throws(
    () => reversePathGeometry({ contours: Array(10001).fill(source.contours[0]) }),
    /budget/i,
  );
  assert.throws(
    () =>
      reversePathGeometry({
        contours: [
          {
            id: 'large',
            closed: false,
            points: Array(100001).fill(point('p', [0, 0])),
          },
        ],
      }),
    /budget/i,
  );
});
