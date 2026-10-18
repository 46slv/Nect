import test from 'node:test';
import assert from 'node:assert/strict';
import { smoothGeometry } from '../src/smooth-geometry.js';

const point = (id, position, handleA = [0, 0], handleB = [0, 0]) => ({
  id,
  position,
  handleA,
  handleB,
});

function openPath(positions) {
  return {
    name: 'source path',
    metadata: { source: 'test' },
    contours: [
      {
        id: 'open',
        closed: false,
        group: 'shape-a',
        points: positions.map((position, index) => point(`p${index}`, position)),
      },
    ],
  };
}

test('uses simultaneous Jacobi neighbor averaging for one and repeated passes', () => {
  const source = openPath([
    [0, 0],
    [10, 4],
    [20, 8],
    [30, 12],
  ]);
  const onePass = smoothGeometry(source, {
    factor: 0.5,
    keepEndpoints: false,
  });
  assert.deepEqual(
    onePass.contours[0].points.map((item) => item.position),
    [
      [5, 2],
      [10, 4],
      [20, 8],
      [25, 10],
    ],
  );

  const twoPasses = smoothGeometry(source, {
    factor: 0.5,
    iterations: 2,
    keepEndpoints: false,
  });
  assert.deepEqual(
    twoPasses.contours[0].points.map((item) => item.position),
    [
      [7.5, 3],
      [11.25, 4.5],
      [18.75, 7.5],
      [22.5, 9],
    ],
  );
});

test('pins open endpoints by default, or moves each toward its sole neighbor', () => {
  const source = openPath([
    [0, 0],
    [10, 20],
    [40, 0],
  ]);
  const pinned = smoothGeometry(source, { factor: 1 });
  assert.deepEqual(
    pinned.contours[0].points.map((item) => item.position),
    [
      [0, 0],
      [20, 0],
      [40, 0],
    ],
  );

  const free = smoothGeometry(source, { factor: 1, keepEndpoints: false });
  assert.deepEqual(
    free.contours[0].points.map((item) => item.position),
    [
      [10, 20],
      [20, 0],
      [10, 20],
    ],
  );

  const singleton = openPath([[7, -3]]);
  assert.deepEqual(
    smoothGeometry(singleton, { factor: 1, keepEndpoints: false }).contours[0].points[0].position,
    [7, -3],
  );
});

test('closed contours use wrapped neighbors and preserve topology and groups', () => {
  const source = {
    contours: [
      {
        id: 'loop',
        closed: true,
        group: 'outer',
        points: [point('a', [0, 0]), point('b', [8, 0]), point('c', [8, 8]), point('d', [0, 8])],
      },
    ],
  };
  const result = smoothGeometry(source, { factor: 1 });
  assert.equal(result.contours[0].closed, true);
  assert.equal(result.contours[0].points.length, 4);
  assert.equal(result.contours[0].group, 'outer');
  assert.deepEqual(
    result.contours[0].points.map((item) => item.position),
    [
      [4, 4],
      [4, 4],
      [4, 4],
      [4, 4],
    ],
  );
});

test('factor zero returns an independent geometry clone without changing handles or source IDs', () => {
  const source = openPath([
    [0, 0],
    [4, 8],
    [10, 0],
  ]);
  source.contours[0].points[1].handleA = [-2, 5];
  source.contours[0].points[1].handleB = [3, -1];
  const before = structuredClone(source);
  const result = smoothGeometry(source, { factor: 0, prefix: 'zero' });

  assert.deepEqual(result, source);
  assert.notEqual(result, source);
  assert.notEqual(result.contours, source.contours);
  assert.notEqual(result.contours[0].points[0].position, source.contours[0].points[0].position);
  result.contours[0].points[0].position[0] = 123;
  assert.deepEqual(source, before);

  const changed = smoothGeometry(source, { factor: 0.5, keepEndpoints: false, prefix: 'blur' });
  assert.ok(changed.contours[0].id.startsWith('blur:'));
  assert.ok(changed.contours[0].points.every((item, index) => item.id === `blur:c0:p${index}`));
  changed.contours[0].points.forEach((item, index) => {
    assert.deepEqual(item.handleA, source.contours[0].points[index].handleA);
    assert.deepEqual(item.handleB, source.contours[0].points[index].handleB);
  });
  assert.deepEqual(source, before);
});

test('rejects invalid options and geometry, and enforces point-iteration work budget', () => {
  const source = openPath([
    [0, 0],
    [1, 2],
    [3, 4],
  ]);
  for (const factor of [-0.1, Infinity, NaN, 1.01])
    assert.throws(() => smoothGeometry(source, { factor }), /factor/i);
  for (const iterations of [0, 1.5, 51, Infinity])
    assert.throws(() => smoothGeometry(source, { factor: 0.5, iterations }), /iterations/i);
  assert.throws(
    () => smoothGeometry(source, { factor: 0.5, keepEndpoints: 'yes' }),
    /keepEndpoints/i,
  );
  assert.throws(() => smoothGeometry(source, { factor: 0.5, prefix: '' }), /prefix/i);
  assert.throws(() => smoothGeometry(source, null), /options/i);
  assert.throws(() => smoothGeometry(null, { factor: 0.5 }), /Path input/);

  const invalid = structuredClone(source);
  invalid.contours[0].points[1].handleB[0] = Infinity;
  assert.throws(() => smoothGeometry(invalid, { factor: 0.5 }), /finite/i);
  assert.throws(
    () => smoothGeometry({ contours: Array(10001).fill(source.contours[0]) }, { factor: 0.5 }),
    /budget/i,
  );

  const tooManyIterations = {
    contours: [
      {
        id: 'large',
        closed: false,
        points: Array.from({ length: 40001 }, (_, index) => point(`p${index}`, [0, 0])),
      },
    ],
  };
  assert.throws(
    () => smoothGeometry(tooManyIterations, { factor: 0.25, iterations: 50 }),
    /work budget/i,
  );
});

test('keeps smoothing finite for extreme finite coordinates', () => {
  const max = Number.MAX_VALUE;
  const source = {
    contours: [
      {
        id: 'extreme',
        closed: true,
        points: [point('a', [max, 0]), point('b', [-max, 0]), point('c', [max, 0])],
      },
    ],
  };
  const result = smoothGeometry(source, { factor: 1 });
  assert.deepEqual(
    result.contours[0].points.map((item) => item.position),
    [
      [0, 0],
      [max, 0],
      [0, 0],
    ],
  );
  assert.ok(result.contours[0].points.every((item) => item.position.every(Number.isFinite)));
});
