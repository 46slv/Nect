import test from 'node:test';
import assert from 'node:assert/strict';
import { setClosureGeometry, validatePathGeometry } from '../src/set-closure-geometry.js';

const fixture = () => ({
  contours: [
    {
      id: 'outer',
      group: 'compound-a',
      closed: false,
      label: { source: 'import' },
      points: [
        { id: 'p0', position: [1, 2], handleA: [-3, -4], handleB: [5, 6] },
        { id: 'p1', position: [20, 30], handleA: [7, 8], handleB: [-9, -10] },
      ],
    },
    {
      id: 'inner',
      group: 'compound-a',
      closed: true,
      points: [{ id: 'p2', position: [100, 200], handleA: [1, 2], handleB: [3, 4] }],
    },
    { id: 'empty', closed: false, points: [] },
  ],
});

test('Set Closure toggles all contours without changing order, groups, anchors or relative handles', () => {
  const source = fixture(),
    before = structuredClone(source),
    closed = setClosureGeometry(source, { closed: true, prefix: 'node-sha' });
  assert.deepEqual(source, before);
  assert.deepEqual(
    closed.contours.map((c) => c.closed),
    [true, true, true],
  );
  assert.deepEqual(
    closed.contours.map((c) => c.group),
    ['compound-a', 'compound-a', undefined],
  );
  assert.deepEqual(
    closed.contours.map((c) =>
      c.points.map(({ position, handleA, handleB }) => ({
        position,
        handleA,
        handleB,
      })),
    ),
    before.contours.map((c) =>
      c.points.map(({ position, handleA, handleB }) => ({
        position,
        handleA,
        handleB,
      })),
    ),
  );
  assert.deepEqual(
    closed.contours.map((c) => c.id),
    ['node-sha:c0', 'node-sha:c1', 'node-sha:c2'],
  );
  assert.deepEqual(
    closed.contours.map((c) => c.points.map((p) => p.id)),
    [['node-sha:c0:p0', 'node-sha:c0:p1'], ['node-sha:c1:p0'], []],
  );
  assert.deepEqual(setClosureGeometry(source, { closed: true, prefix: 'node-sha' }), closed);
  assert.notEqual(closed.contours[0], source.contours[0]);
  assert.notEqual(closed.contours[0].points[0], source.contours[0].points[0]);
  assert.notEqual(closed.contours[0].points[0].position, source.contours[0].points[0].position);

  const opened = setClosureGeometry(source, { closed: false, prefix: 'node-sha' });
  const restored = setClosureGeometry(opened, { closed: true, prefix: 'node-sha' });
  assert.deepEqual(restored.contours, closed.contours);
  assert.deepEqual(
    opened.contours.map((c) => c.closed),
    [false, false, false],
  );
});

test('Set Closure explicitly accepts empty paths and one-point contours', () => {
  assert.deepEqual(setClosureGeometry({ contours: [] }, { closed: false }).contours, []);
  const one = {
    contours: [
      {
        id: 'one',
        closed: false,
        points: [{ id: 'p', position: [0, 0], handleA: [-2, -3], handleB: [4, 5] }],
      },
    ],
  };
  const result = setClosureGeometry(one, { closed: true, prefix: 'single' });
  assert.equal(result.contours[0].closed, true);
  assert.deepEqual(result.contours[0].points[0], {
    id: 'single:c0:p0',
    position: [0, 0],
    handleA: [-2, -3],
    handleB: [4, 5],
  });
  assert.deepEqual(validatePathGeometry(one), { contourCount: 1, pointCount: 1 });
});

test('Set Closure rejects malformed/non-finite geometry and out-of-budget paths', () => {
  const base = fixture();
  const badCoordinate = structuredClone(base);
  badCoordinate.contours[0].points[0].position[0] = Infinity;
  assert.throws(() => setClosureGeometry(badCoordinate), /finite positions and handles/);

  const badControlArithmetic = {
    contours: [
      {
        id: 'overflow',
        closed: false,
        points: [
          {
            id: 'p',
            position: [Number.MAX_VALUE, 0],
            handleA: [Number.MAX_VALUE, 0],
            handleB: [0, 0],
          },
        ],
      },
    ],
  };
  assert.throws(() => setClosureGeometry(badControlArithmetic), /finite-coordinate arithmetic/);
  assert.throws(
    () => setClosureGeometry({ contours: [{ id: 'missing', closed: false }] }),
    /valid contours/,
  );
  assert.throws(() => setClosureGeometry(base, { closed: 1 }), /Boolean Closed/);
  assert.throws(() => setClosureGeometry(base, { prefix: '' }), /nonempty ID prefix/);

  const tooManyContours = {
    contours: Array.from({ length: 10001 }, (_, i) => ({ id: `c${i}`, closed: false, points: [] })),
  };
  assert.throws(() => validatePathGeometry(tooManyContours), /exceeds geometry budget/);
  const tooManyPoints = {
    contours: [
      {
        id: 'c',
        closed: false,
        points: Array.from({ length: 100001 }, (_, i) => ({
          id: `p${i}`,
          position: [0, 0],
          handleA: [0, 0],
          handleB: [0, 0],
        })),
      },
    ],
  };
  assert.throws(() => validatePathGeometry(tooManyPoints), /exceeds geometry budget/);
});
