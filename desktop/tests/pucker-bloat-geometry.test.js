import test from 'node:test';
import assert from 'node:assert/strict';
import { puckerBloatGeometry } from '../src/pucker-bloat-geometry.js';

const point = (id, position, handleA = [0, 0], handleB = [0, 0]) => ({
  id,
  position,
  handleA,
  handleB,
});

function square() {
  return {
    name: 'square',
    metadata: { source: 'test' },
    contours: [
      {
        id: 'outer',
        closed: true,
        group: 'shape-a',
        points: [
          point('p0', [10, 10], [0, -2], [2, 0]),
          point('p1', [20, 10]),
          point('p2', [20, 20]),
          point('p3', [10, 20]),
        ],
      },
    ],
  };
}

function absoluteControl(anchor, relativeHandle) {
  return [anchor[0] + relativeHandle[0], anchor[1] + relativeHandle[1]];
}

test('positive Bloat and negative Pucker scale square anchors and absolute handles oppositely', () => {
  const source = square();
  const before = structuredClone(source);
  const bloat = puckerBloatGeometry(source, { amount: 50, autoCenter: true, prefix: 'bloat' });
  const pucker = puckerBloatGeometry(source, { amount: -50, autoCenter: true, prefix: 'pucker' });
  const original = source.contours[0].points[0];
  const bloated = bloat.contours[0].points[0];
  const puckered = pucker.contours[0].points[0];

  assert.deepEqual(bloated.position, [12.5, 12.5]);
  assert.deepEqual(absoluteControl(bloated.position, bloated.handleA), [7.5, 4.5]);
  assert.deepEqual(absoluteControl(bloated.position, bloated.handleB), [10.5, 7.5]);
  assert.deepEqual(bloated.handleA, [-5, -8]);
  assert.deepEqual(bloated.handleB, [-2, -5]);

  assert.deepEqual(puckered.position, [7.5, 7.5]);
  assert.deepEqual(absoluteControl(puckered.position, puckered.handleA), [12.5, 11.5]);
  assert.deepEqual(absoluteControl(puckered.position, puckered.handleB), [13.5, 12.5]);
  assert.deepEqual(puckered.handleA, [5, 4]);
  assert.deepEqual(puckered.handleB, [6, 5]);
  assert.deepEqual(absoluteControl(original.position, original.handleA), [10, 8]);
  assert.deepEqual(absoluteControl(original.position, original.handleB), [12, 10]);
  assert.deepEqual(source, before);
});

test('automatic anchor-bounds center matches an explicit manual center', () => {
  const source = square();
  const automatic = puckerBloatGeometry(source, { amount: 25, autoCenter: true });
  const manual = puckerBloatGeometry(source, {
    amount: 25,
    autoCenter: false,
    center: [15, 15],
  });
  const ignoredManualCenter = puckerBloatGeometry(source, {
    amount: 25,
    autoCenter: true,
    center: [100, -100],
  });
  assert.deepEqual(automatic.contours[0].points, manual.contours[0].points);
  assert.deepEqual(automatic.contours[0].points, ignoredManualCenter.contours[0].points);

  const offCenter = puckerBloatGeometry(source, {
    amount: 25,
    autoCenter: false,
    center: [0, 0],
  });
  assert.notDeepEqual(
    offCenter.contours[0].points[0].position,
    automatic.contours[0].points[0].position,
  );
  assert.deepEqual(offCenter.contours[0].points[0].position, [7.5, 7.5]);
});

test('straight polygon edges acquire nonzero relative handles for nonzero amounts', () => {
  const source = {
    contours: [
      {
        id: 'triangle',
        closed: true,
        points: [point('a', [0, 0]), point('b', [8, 0]), point('c', [0, 8])],
      },
    ],
  };
  const result = puckerBloatGeometry(source, { amount: 50, autoCenter: true });
  const transformed = result.contours[0].points[0];
  assert.notDeepEqual(transformed.handleA, [0, 0]);
  assert.notDeepEqual(transformed.handleB, [0, 0]);
});

test('preserves contour order, closed flags and groups with deterministic namespaced IDs', () => {
  const source = square();
  source.contours.push({
    id: 'open',
    closed: false,
    group: 'shape-a',
    points: [point('q0', [12, 13]), point('q1', [14, 17])],
  });
  const first = puckerBloatGeometry(source, { amount: 20, autoCenter: true, prefix: 'pb' });
  const second = puckerBloatGeometry(source, { amount: 20, autoCenter: true, prefix: 'pb' });
  assert.deepEqual(first, second);
  assert.deepEqual(
    first.contours.map(({ closed, group }) => ({ closed, group })),
    [
      { closed: true, group: 'shape-a' },
      { closed: false, group: 'shape-a' },
    ],
  );
  assert.deepEqual(
    first.contours.map((contour) => contour.id),
    ['pb:c0', 'pb:c1'],
  );
  assert.deepEqual(
    first.contours.map((contour) => contour.points.map((item) => item.id)),
    [
      ['pb:c0:p0', 'pb:c0:p1', 'pb:c0:p2', 'pb:c0:p3'],
      ['pb:c1:p0', 'pb:c1:p1'],
    ],
  );
  assert.equal(
    new Set(first.contours.flatMap((contour) => contour.points.map((item) => item.id))).size,
    6,
  );
});

test('zero amount returns a geometry-equivalent independent clone', () => {
  const source = square();
  const before = structuredClone(source);
  const result = puckerBloatGeometry(source, { amount: 0, autoCenter: true, prefix: 'zero' });
  assert.deepEqual(result, source);
  assert.notEqual(result, source);
  assert.notEqual(result.contours, source.contours);
  assert.notEqual(result.contours[0], source.contours[0]);
  assert.notEqual(result.contours[0].points[0].position, source.contours[0].points[0].position);
  result.contours[0].points[0].position[0] = 999;
  result.contours[0].points[0].handleA[0] = 999;
  assert.deepEqual(source, before);
});

test('supports empty geometry and contours without points', () => {
  const empty = { name: 'empty', contours: [] };
  const result = puckerBloatGeometry(empty, { amount: 50, autoCenter: true });
  assert.deepEqual(result, empty);
  assert.notEqual(result, empty);
  assert.notEqual(result.contours, empty.contours);

  const noPoints = {
    contours: [
      { id: 'blank', group: 'g', closed: false, points: [] },
      { id: 'also-blank', group: 'g', closed: true, points: [] },
    ],
  };
  assert.deepEqual(
    puckerBloatGeometry(noPoints, { amount: 50, autoCenter: true, prefix: 'empty' }).contours,
    [
      { id: 'empty:c0', group: 'g', closed: false, points: [] },
      { id: 'empty:c1', group: 'g', closed: true, points: [] },
    ],
  );
});

test('rejects invalid amounts, options, centers and prefixes', () => {
  const source = square();
  for (const amount of [-100.01, 100.01, Infinity, -Infinity, NaN, undefined])
    assert.throws(() => puckerBloatGeometry(source, { amount, autoCenter: true }), /amount/i);
  for (const autoCenter of [undefined, null, 0, 'yes'])
    assert.throws(() => puckerBloatGeometry(source, { amount: 10, autoCenter }), /autoCenter/i);
  assert.throws(() => puckerBloatGeometry(source, { amount: 10, autoCenter: false }), /center/i);
  for (const center of [null, [0], [0, 0, 0], [0, Infinity], '0,0']) {
    assert.throws(
      () => puckerBloatGeometry(source, { amount: 10, autoCenter: false, center }),
      /center/i,
    );
    assert.throws(
      () => puckerBloatGeometry(source, { amount: 10, autoCenter: true, center }),
      /center/i,
    );
  }
  for (const prefix of ['', null, 2])
    assert.throws(
      () => puckerBloatGeometry(source, { amount: 10, autoCenter: true, prefix }),
      /prefix/i,
    );
  assert.throws(() => puckerBloatGeometry(source, null), /options/i);
  assert.throws(() => puckerBloatGeometry(source, []), /options/i);
  assert.throws(() => puckerBloatGeometry(source, { amount: 10 }), /autoCenter/i);
});

test('rejects malformed input and enforces contour and point budgets', () => {
  const source = square();
  assert.throws(() => puckerBloatGeometry(null, { amount: 10, autoCenter: true }), /Path input/i);
  assert.throws(
    () =>
      puckerBloatGeometry(
        { contours: Array(10001).fill(source.contours[0]) },
        { amount: 10, autoCenter: true },
      ),
    /budget/i,
  );
  const tooManyPoints = {
    contours: [
      {
        id: 'large',
        closed: true,
        points: Array(100001).fill(point('p', [0, 0])),
      },
    ],
  };
  assert.throws(
    () => puckerBloatGeometry(tooManyPoints, { amount: 10, autoCenter: true }),
    /budget/i,
  );

  const invalid = structuredClone(source);
  invalid.contours[0].points[0].handleB[0] = Infinity;
  assert.throws(() => puckerBloatGeometry(invalid, { amount: 10, autoCenter: true }), /finite/i);
  const invalidContour = structuredClone(source);
  invalidContour.contours[0].closed = 'yes';
  assert.throws(
    () => puckerBloatGeometry(invalidContour, { amount: 10, autoCenter: true }),
    /contours/i,
  );
});

test('rejects nonfinite absolute-control or transform arithmetic', () => {
  const absoluteControlOverflow = {
    contours: [
      {
        id: 'overflow',
        closed: false,
        points: [point('p', [1e308, 0], [1e308, 0])],
      },
    ],
  };
  assert.throws(
    () =>
      puckerBloatGeometry(absoluteControlOverflow, {
        amount: 50,
        autoCenter: false,
        center: [0, 0],
      }),
    /finite-coordinate arithmetic/i,
  );

  const transformOverflow = {
    contours: [
      {
        id: 'overflow',
        closed: false,
        points: [point('p', [1e308, 0])],
      },
    ],
  };
  assert.throws(
    () =>
      puckerBloatGeometry(transformOverflow, {
        amount: -10,
        autoCenter: false,
        center: [-1e308, 0],
      }),
    /finite-coordinate arithmetic/i,
  );
});
