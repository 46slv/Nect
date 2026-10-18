import test from 'node:test';
import assert from 'node:assert/strict';
import { instancePointsGeometry } from '../src/instance-points-geometry.js';

const point = (id, position, handleA = [0, 0], handleB = [0, 0], extra = {}) => ({
  id,
  position,
  handleA,
  handleB,
  ...extra,
});
const contour = (id, group, points, closed = true, extra = {}) => ({
  id,
  ...(group === undefined ? {} : { group }),
  closed,
  points,
  ...extra,
});

const sourcePath = () => ({
  name: 'source paint and metadata',
  paint: { fill: '#e34b2f', opacity: 0.7 },
  contours: [
    contour(
      'outer',
      'shape',
      [
        point('a', [0, 0], [-1, 2], [3, 4], { smooth: true }),
        point('b', [12, 0], [0, 0], [2, -1]),
        point('c', [12, 12]),
      ],
      true,
      { role: 'outline' },
    ),
    contour('hole', 'shape', [point('d', [3, 3]), point('e', [4, 3]), point('f', [4, 4])]),
    contour('other', 'other-group', [point('g', [30, 5])], false),
    contour('implicit', undefined, [point('h', [40, 5])], false),
  ],
});
const targetPath = () => ({
  paint: { fill: 'target paint must not leak' },
  contours: [
    contour(
      'target-a',
      'target-group',
      [
        point('ta', [100, 50], [-90, 0], [90, 0], { tangent: [0, 20] }),
        point('tb', [100, 50], [0, -90], [0, 90], { tangent: [20, 0] }),
      ],
      false,
    ),
    contour('target-b', 'another-target-group', [point('tc', [5, -10], [200, 0], [-200, 0])], true),
  ],
});

function absoluteHandle(pointValue, handleName) {
  return pointValue.position.map((value, axis) => value + pointValue[handleName][axis]);
}

function assertVectorClose(actual, expected, epsilon = 1e-9) {
  assert.equal(actual.length, expected.length);
  actual.forEach((value, axis) => assert.ok(Math.abs(value - expected[axis]) <= epsilon));
}

test('translates every source point from the supplied pivot and retains relative handles', () => {
  const source = sourcePath();
  const targets = targetPath();
  const pivot = [10, 4];
  const result = instancePointsGeometry(source, targets, { pivot, prefix: 'scatter' });
  const expectedAnchors = [
    [100, 50],
    [100, 50],
    [5, -10],
  ];
  const expectedGroups = [0, 0, 1, 2];

  assert.equal(result.contours.length, source.contours.length * expectedAnchors.length);
  expectedAnchors.forEach((anchor, instanceIndex) => {
    source.contours.forEach((sourceContour, contourIndex) => {
      const actualContour = result.contours[instanceIndex * source.contours.length + contourIndex];
      assert.equal(actualContour.closed, sourceContour.closed);
      assert.equal(actualContour.role, sourceContour.role);
      assert.equal(
        actualContour.group,
        `scatter:i${instanceIndex}:g${expectedGroups[contourIndex]}`,
      );
      sourceContour.points.forEach((sourcePoint, pointIndex) => {
        const actual = actualContour.points[pointIndex];
        const delta = [anchor[0] - pivot[0], anchor[1] - pivot[1]];
        assert.deepEqual(actual.position, [
          sourcePoint.position[0] + delta[0],
          sourcePoint.position[1] + delta[1],
        ]);
        assert.deepEqual(actual.handleA, sourcePoint.handleA);
        assert.deepEqual(actual.handleB, sourcePoint.handleB);
        assert.deepEqual(absoluteHandle(actual, 'handleA'), [
          sourcePoint.position[0] + delta[0] + sourcePoint.handleA[0],
          sourcePoint.position[1] + delta[1] + sourcePoint.handleA[1],
        ]);
        assert.deepEqual(absoluteHandle(actual, 'handleB'), [
          sourcePoint.position[0] + delta[0] + sourcePoint.handleB[0],
          sourcePoint.position[1] + delta[1] + sourcePoint.handleB[1],
        ]);
      });
    });
  });
  assert.equal(result.paint.fill, source.paint.fill);
  assert.equal(result.paint.fill, '#e34b2f');
  assert.notEqual(result.paint.fill, targets.paint.fill);
  assert.deepEqual(
    result.contours.map((c) => c.id),
    [
      'scatter:i0:c0',
      'scatter:i0:c1',
      'scatter:i0:c2',
      'scatter:i0:c3',
      'scatter:i1:c0',
      'scatter:i1:c1',
      'scatter:i1:c2',
      'scatter:i1:c3',
      'scatter:i2:c0',
      'scatter:i2:c1',
      'scatter:i2:c2',
      'scatter:i2:c3',
    ],
  );
});

test('default and explicit alignTangent false preserve translation-only output exactly', () => {
  const source = sourcePath();
  const targets = targetPath();
  const defaultResult = instancePointsGeometry(source, targets, { pivot: [3, -7], prefix: 'same' });
  const explicitFalse = instancePointsGeometry(source, targets, {
    pivot: [3, -7],
    prefix: 'same',
    alignTangent: false,
  });
  assert.deepEqual(explicitFalse, defaultResult);
});

test('aligns source +X to horizontal, bent, and vertical target anchor tangents', () => {
  const source = {
    contours: [contour('source', 'ornament', [point('p', [2, 1], [1, 0], [0, 2])], false)],
  };
  const targets = {
    contours: [
      contour(
        'path',
        undefined,
        [point('a', [0, 0]), point('b', [10, 0]), point('c', [10, 10])],
        false,
      ),
    ],
  };
  const beforeTargets = structuredClone(targets);
  const result = instancePointsGeometry(source, targets, {
    pivot: [0, 0],
    alignTangent: true,
  });
  const copies = result.contours.map((copy) => copy.points[0]);

  assert.deepEqual(copies[0].position, [2, 1]);
  assert.deepEqual(copies[0].handleA, [1, 0]);
  assert.deepEqual(copies[0].handleB, [0, 2]);
  assertVectorClose(copies[1].position, [10 + Math.sqrt(0.5), Math.sqrt(4.5)]);
  assertVectorClose(copies[1].handleA, [Math.sqrt(0.5), Math.sqrt(0.5)]);
  assertVectorClose(copies[1].handleB, [-Math.sqrt(2), Math.sqrt(2)]);
  assert.deepEqual(copies[2].position, [9, 12]);
  assertVectorClose(copies[2].handleA, [0, 1]);
  assertVectorClose(copies[2].handleB, [-2, 0]);
  assert.deepEqual(targets, beforeTargets);
});

test('prefers outgoing then reverse incoming Bezier handles for target tangents', () => {
  const source = {
    contours: [contour('source', undefined, [point('p', [1, 0], [2, 3], [4, 5])], false)],
  };
  const targets = {
    contours: [
      contour(
        'path',
        undefined,
        [point('outgoing', [0, 0], [0, 0], [0, 1]), point('incoming', [10, 0], [1, 0], [0, 0])],
        false,
      ),
    ],
  };
  const copies = instancePointsGeometry(source, targets, {
    pivot: [0, 0],
    alignTangent: true,
  }).contours.map((copy) => copy.points[0]);

  assertVectorClose(copies[0].position, [0, 1]);
  assertVectorClose(copies[0].handleA, [-3, 2]);
  assertVectorClose(copies[0].handleB, [-5, 4]);
  assertVectorClose(copies[1].position, [9, 0]);
  assertVectorClose(copies[1].handleA, [-2, -3]);
  assertVectorClose(copies[1].handleB, [-4, -5]);
});

test('uses wrapped neighbors for tangent alignment at a closed contour seam', () => {
  const source = { contours: [contour('source', undefined, [point('p', [1, 0])], false)] };
  const targets = {
    contours: [
      contour(
        'closed-path',
        undefined,
        [point('seam', [0, 0]), point('right', [10, 0]), point('up', [10, 10])],
        true,
      ),
    ],
  };
  const result = instancePointsGeometry(source, targets, {
    pivot: [0, 0],
    alignTangent: true,
  });
  assertVectorClose(result.contours[0].points[0].position, [0, -1]);
});

test('falls back from a zero centered chord to an adjacent edge and then to +X', () => {
  const source = {
    name: 'repeated ornament',
    paint: { fill: '#123456' },
    contours: [contour('source', undefined, [point('p', [1, 0])], false)],
  };
  const cusp = {
    contours: [
      contour(
        'cusp',
        undefined,
        [point('left', [0, 0]), point('center', [10, 0]), point('return', [0, 0])],
        false,
      ),
    ],
  };
  const cuspCenter = instancePointsGeometry(source, cusp, {
    pivot: [0, 0],
    alignTangent: true,
  }).contours[1].points[0];
  assertVectorClose(cuspCenter.position, [9, 0]);

  const duplicates = {
    contours: [
      contour(
        'duplicates',
        undefined,
        [point('a', [5, 6]), point('b', [5, 6]), point('c', [5, 6])],
        false,
      ),
    ],
  };
  const duplicateResult = instancePointsGeometry(source, duplicates, {
    pivot: [0, 0],
    alignTangent: true,
  });
  const duplicateCopies = duplicateResult.contours.map((copy) => copy.points[0]);
  for (const copy of duplicateCopies) assert.deepEqual(copy.position, [6, 6]);
  assert.deepEqual(
    duplicateResult.contours.map((copy) => copy.id),
    ['instances:i0:c0', 'instances:i1:c0', 'instances:i2:c0'],
  );
  assert.equal(new Set(duplicateResult.contours.map((copy) => copy.group)).size, 3);
  assert.equal(duplicateResult.name, source.name);
  assert.deepEqual(duplicateResult.paint, source.paint);
});

test('preflights aligned arithmetic and rolls back on overflow without mutating inputs', () => {
  const source = {
    contours: [contour('source', undefined, [point('p', [1.7e308, 0])], false)],
  };
  const targets = {
    contours: [
      contour(
        'target',
        undefined,
        [
          point('safe', [0, 0], [0, 0], [1, 0]),
          point('overflow', [1.7e308, 1.7e308], [0, 0], [1, 1]),
        ],
        false,
      ),
    ],
  };
  const beforeSource = structuredClone(source);
  const beforeTargets = structuredClone(targets);
  assert.throws(
    () => instancePointsGeometry(source, targets, { pivot: [0, 0], alignTangent: true }),
    /finite coordinates/i,
  );
  assert.deepEqual(source, beforeSource);
  assert.deepEqual(targets, beforeTargets);
});

test('keeps anchor order, source contour/point order, holes, and orientation unchanged', () => {
  const source = sourcePath();
  const targets = targetPath();
  const result = instancePointsGeometry(source, targets, { pivot: [0, 0], prefix: 'ordered' });
  for (let instanceIndex = 0; instanceIndex < 3; instanceIndex++) {
    const copy = result.contours.slice(instanceIndex * 4, instanceIndex * 4 + 4);
    assert.deepEqual(
      copy.map((c) => c.id.split(':').at(-1)),
      ['c0', 'c1', 'c2', 'c3'],
    );
    assert.deepEqual(
      copy[0].points.map((p) => p.id.split(':').at(-1)),
      ['p0', 'p1', 'p2'],
    );
    const actualAnchor = instanceIndex < 2 ? [100, 50] : [5, -10];
    assert.deepEqual(
      copy[0].points.map((p) => p.position),
      source.contours[0].points.map((p) => [
        p.position[0] + actualAnchor[0],
        p.position[1] + actualAnchor[1],
      ]),
    );
  }
  assert.deepEqual(
    result.contours.filter((c) => c.group === 'ordered:i0:g0').map((c) => c.id),
    ['ordered:i0:c0', 'ordered:i0:c1'],
  );
});

test('coincident target anchors produce distinct copies with unique identities and groups', () => {
  const source = { contours: [contour('single', undefined, [point('p', [2, 3])], false)] };
  const targets = {
    contours: [contour('points', undefined, [point('a', [7, 9]), point('b', [7, 9])], false)],
  };
  const result = instancePointsGeometry(source, targets, { pivot: [2, 3] });
  assert.deepEqual(
    result.contours.map((c) => c.points[0].position),
    [
      [7, 9],
      [7, 9],
    ],
  );
  assert.equal(new Set(result.contours.map((c) => c.id)).size, 2);
  assert.equal(new Set(result.contours.map((c) => c.points[0].id)).size, 2);
  assert.equal(new Set(result.contours.map((c) => c.group)).size, 2);
});

test('is deterministic, preserves source-only metadata, and does not mutate or share geometry arrays', () => {
  const source = sourcePath();
  const targets = targetPath();
  const beforeSource = structuredClone(source);
  const beforeTargets = structuredClone(targets);
  const options = { pivot: [1, 2], prefix: 'repeat' };
  const first = instancePointsGeometry(source, targets, options);
  assert.deepEqual(instancePointsGeometry(source, targets, options), first);
  assert.deepEqual(source, beforeSource);
  assert.deepEqual(targets, beforeTargets);
  assert.deepEqual(first.name, source.name);
  assert.equal(first.paint.fill, source.paint.fill);
  assert.equal(Object.hasOwn(first, 'targetPaint'), false);
  assert.notEqual(first.contours[0], source.contours[0]);
  assert.notEqual(first.contours[0].points[0].position, source.contours[0].points[0].position);
  assert.notEqual(first.contours[0].points[0].handleA, source.contours[0].points[0].handleA);
  assert.notEqual(first.paint, source.paint);
  first.contours[0].points[0].position[0] = 999;
  first.paint.opacity = 0;
  assert.equal(source.contours[0].points[0].position[0], 0);
  assert.equal(source.paint.opacity, 0.7);
});

test('returns empty source-shaped paths for empty source or targets without requiring a pivot', () => {
  const target = targetPath();
  const emptySource = { name: 'empty source', paint: { fill: 'blue' }, contours: [] };
  const noSourceAnchors = { contours: [contour('blank', 'blank-group', [], true)] };
  const noTargetAnchors = { contours: [contour('blank-target', undefined, [], false)] };
  const emptyResults = [
    instancePointsGeometry(emptySource, target),
    instancePointsGeometry(noSourceAnchors, target),
    instancePointsGeometry(sourcePath(), noTargetAnchors),
  ];
  for (const result of emptyResults) assert.deepEqual(result.contours, []);
  assert.equal(emptyResults[0].paint.fill, 'blue');
  assert.equal(emptyResults[0].name, 'empty source');
});

test('validates inputs, options, pivot, and finite arithmetic', () => {
  const source = { contours: [contour('c', undefined, [point('p', [0, 0], [1, 0])])] };
  const target = { contours: [contour('t', undefined, [point('q', [4, 5])])] };
  for (const invalidPivot of [undefined, null, [1], [1, Infinity], [NaN, 0], '0,0']) {
    assert.throws(() => instancePointsGeometry(source, target, { pivot: invalidPivot }), /pivot/i);
  }
  assert.throws(() => instancePointsGeometry(source, target, null), /options/i);
  assert.throws(
    () => instancePointsGeometry(source, target, { pivot: [0, 0], alignTangent: 'yes' }),
    /alignTangent/i,
  );
  assert.throws(
    () => instancePointsGeometry(source, target, { pivot: [0, 0], prefix: '' }),
    /prefix/i,
  );
  assert.throws(() => instancePointsGeometry(null, target, { pivot: [0, 0] }), /source.*Path/i);
  assert.throws(() => instancePointsGeometry(source, null, { pivot: [0, 0] }), /targets.*Path/i);

  const duplicateIdSource = { contours: [source.contours[0], { ...source.contours[0], id: 'c' }] };
  assert.throws(
    () => instancePointsGeometry(duplicateIdSource, target, { pivot: [0, 0] }),
    /contours.*IDs/i,
  );
  const invalidTarget = structuredClone(target);
  invalidTarget.contours[0].points[0].position[0] = Infinity;
  assert.throws(() => instancePointsGeometry(source, invalidTarget, { pivot: [0, 0] }), /finite/i);
  const invalidAbsoluteHandle = structuredClone(source);
  invalidAbsoluteHandle.contours[0].points[0].position[0] = Number.MAX_VALUE;
  invalidAbsoluteHandle.contours[0].points[0].handleA[0] = Number.MAX_VALUE;
  assert.throws(
    () => instancePointsGeometry(invalidAbsoluteHandle, target, { pivot: [0, 0] }),
    /finite-coordinate/i,
  );
  const overflowDelta = {
    contours: [contour('t', undefined, [point('q', [Number.MAX_VALUE, 0])])],
  };
  assert.throws(
    () => instancePointsGeometry(source, overflowDelta, { pivot: [-Number.MAX_VALUE, 0] }),
    /finite/i,
  );
  const overflowOutput = {
    contours: [contour('s', undefined, [point('p', [Number.MAX_VALUE, 0])])],
  };
  const largeTarget = { contours: [contour('t', undefined, [point('q', [1e308, 0])])] };
  assert.throws(
    () => instancePointsGeometry(overflowOutput, largeTarget, { pivot: [0, 0] }),
    /finite/i,
  );
});

test('rejects input, instance, output, group, and work budgets rather than truncating', () => {
  const smallSource = { contours: [contour('s', 'g', [point('p', [0, 0])])] };
  const targets = (count) => ({
    contours: [
      contour(
        't',
        undefined,
        Array.from({ length: count }, (_, i) => point(`p${i}`, [i, 0])),
      ),
    ],
  });
  const largeContourInput = {
    contours: Array.from({ length: 10_001 }, (_, i) => contour(`c${i}`, undefined, [])),
  };
  assert.throws(() => instancePointsGeometry(largeContourInput, targets(0)), /source.*budget/i);
  const largePointInput = {
    contours: [
      contour(
        'c',
        undefined,
        Array.from({ length: 100_001 }, (_, i) => point(`p${i}`, [0, 0])),
      ),
    ],
  };
  assert.throws(() => instancePointsGeometry(smallSource, largePointInput), /targets.*budget/i);
  assert.throws(
    () => instancePointsGeometry(smallSource, targets(257), { pivot: [0, 0] }),
    /instance budget/i,
  );

  const twoGroups = {
    contours: [contour('a', 'a', [point('pa', [0, 0])]), contour('b', 'b', [point('pb', [0, 0])])],
  };
  assert.throws(
    () => instancePointsGeometry(twoGroups, targets(129), { pivot: [0, 0] }),
    /geometry budget/i,
  );
  const tooManyOutputContours = {
    contours: Array.from({ length: 40 }, (_, i) => contour(`c${i}`, 'g', [point(`p${i}`, [0, 0])])),
  };
  assert.throws(
    () => instancePointsGeometry(tooManyOutputContours, targets(256), { pivot: [0, 0] }),
    /geometry budget/i,
  );
  const tooManyOutputPoints = {
    contours: [
      contour(
        'c',
        'g',
        Array.from({ length: 400 }, (_, i) => point(`p${i}`, [0, 0])),
      ),
    ],
  };
  assert.throws(
    () => instancePointsGeometry(tooManyOutputPoints, targets(256), { pivot: [0, 0] }),
    /geometry budget/i,
  );
});
