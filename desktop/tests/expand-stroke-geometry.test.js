import test from 'node:test';
import assert from 'node:assert/strict';
import { expandStrokeGeometry } from '../src/expand-stroke-geometry.js';
import { flattenCubicCenterline } from '../src/closed-offset.js';
import { NectClipperOffset } from '../src/clipper-adapter.js';

const point = (id, position, handleA = [0, 0], handleB = [0, 0]) => ({
  id,
  position,
  handleA,
  handleB,
});
const contour = (id, positions, { closed = false, group, handles } = {}) => ({
  id,
  closed,
  ...(group === undefined ? {} : { group }),
  points: positions.map((position, index) =>
    point(`${id}-p${index}`, position, ...(handles?.[index] || [])),
  ),
});
const stroked = (contours, style = {}) => ({
  contours,
  stroke: { width: 10, cap: 'butt', join: 'miter', miterLimit: 4, dash: [], ...style },
});
const bounds = (path) => {
  const positions = path.contours.flatMap((item) => item.points.map((p) => p.position));
  return [
    Math.min(...positions.map(([x]) => x)),
    Math.min(...positions.map(([, y]) => y)),
    Math.max(...positions.map(([x]) => x)),
    Math.max(...positions.map(([, y]) => y)),
  ];
};
const signedArea = (item) => {
  let sum = 0;
  for (let index = 0; index < item.points.length; index++) {
    const [x1, y1] = item.points[index].position;
    const [x2, y2] = item.points[(index + 1) % item.points.length].position;
    sum += x1 * y2 - y1 * x2;
  }
  return sum / 2;
};
const circleContour = (id, radius, { group } = {}) => {
  const handle = (4 * (Math.SQRT2 - 1) * radius) / 3;
  return {
    id,
    closed: true,
    ...(group === undefined ? {} : { group }),
    points: [
      point(`${id}-p0`, [radius, 0], [0, -handle], [0, handle]),
      point(`${id}-p1`, [0, radius], [handle, 0], [-handle, 0]),
      point(`${id}-p2`, [-radius, 0], [0, handle], [0, -handle]),
      point(`${id}-p3`, [0, -radius], [-handle, 0], [handle, 0]),
    ],
  };
};

test('expands open lines with butt, square, and round cap bounds', () => {
  const source = [
    contour('line', [
      [0, 0],
      [20, 0],
    ]),
  ];
  assert.deepEqual(
    bounds(expandStrokeGeometry(stroked(source), { prefix: 'butt' })),
    [0, -5, 20, 5],
  );
  assert.deepEqual(
    bounds(expandStrokeGeometry(stroked(source, { cap: 'square' }), { prefix: 'square' })),
    [-5, -5, 25, 5],
  );
  assert.deepEqual(
    bounds(expandStrokeGeometry(stroked(source, { cap: 'round' }), { prefix: 'round' })).map(
      (coordinate, index) => Math.abs(coordinate - [-5, -5, 25, 5][index]) <= 0.1,
    ),
    [true, true, true, true],
  );
});

test('flattens bounded cubic centerlines and aligns open caps to endpoint tangents', () => {
  const curve = [
    contour(
      'curve',
      [
        [0, 0],
        [20, 20],
      ],
      {
        handles: [
          [undefined, [0, 10]],
          [[-10, 0], undefined],
        ],
      },
    ),
  ];
  const butt = expandStrokeGeometry(stroked(curve, { width: 10, cap: 'butt' }));
  const square = expandStrokeGeometry(stroked(curve, { width: 10, cap: 'square' }));
  const round = expandStrokeGeometry(stroked(curve, { width: 10, cap: 'round' }));
  const hasPoint = (path, target, tolerance = 0.001) =>
    path.contours.some((item) =>
      item.points.some(
        ({ position: [x, y] }) =>
          Math.abs(x - target[0]) <= tolerance && Math.abs(y - target[1]) <= tolerance,
      ),
    );
  assert.ok(
    hasPoint(butt, [-5, 0]),
    'butt start cap is perpendicular to the vertical start tangent',
  );
  assert.ok(hasPoint(butt, [5, 0]));
  assert.ok(hasPoint(square, [-5, -5]), 'square start cap extends backward along the tangent');
  assert.ok(hasPoint(square, [5, -5]));
  assert.ok(hasPoint(square, [25, 15]), 'square end cap extends along the horizontal end tangent');
  assert.ok(hasPoint(square, [25, 25]));
  assert.ok(hasPoint(round, [5, 0]), 'round cap joins the same tangent-normal boundary');
  assert.ok(
    hasPoint(round, [0, -5], 0.2),
    'round cap is a polygonal semicircle behind the tangent',
  );
  assert.deepEqual(bounds(butt), [-5, 0, 20, 25]);
  assert.deepEqual(bounds(square), [-5, -5, 25, 25]);
  assert.ok(bounds(round).every((value, i) => Math.abs(value - [-5, -5, 25, 25][i]) <= 0.1));
});

test('miter and bevel joins produce distinct corners on a straight-edge turn', () => {
  const source = [
    contour('elbow', [
      [0, 0],
      [20, 0],
      [20, 20],
    ]),
  ];
  const miter = expandStrokeGeometry(stroked(source, { width: 10, join: 'miter' }));
  const bevel = expandStrokeGeometry(stroked(source, { width: 10, join: 'bevel' }));
  const hasPoint = (path, target) =>
    path.contours.some((item) =>
      item.points.some(
        ({ position }) =>
          Math.abs(position[0] - target[0]) < 1e-9 && Math.abs(position[1] - target[1]) < 1e-9,
      ),
    );
  assert.ok(hasPoint(miter, [25, -5]), 'miter reaches the 45-degree intersection');
  assert.ok(!hasPoint(bevel, [25, -5]), 'bevel clips off the miter tip');
  assert.ok(Math.abs(signedArea(miter.contours[0])) > Math.abs(signedArea(bevel.contours[0])));
});

test('closed rectangle stroke is a centered band with a correctly wound hole', () => {
  const source = [
    contour(
      'rect',
      [
        [0, 0],
        [20, 0],
        [20, 10],
        [0, 10],
      ],
      { closed: true },
    ),
  ];
  const expanded = expandStrokeGeometry(stroked(source, { width: 2 }));
  assert.equal(expanded.contours.length, 2);
  assert.deepEqual(expanded.contours.map((item) => Math.sign(signedArea(item))).sort(), [-1, 1]);
  assert.equal(
    expanded.contours.reduce((sum, item) => sum + signedArea(item), 0),
    120,
  );
  assert.deepEqual(bounds(expanded), [-1, -1, 21, 11]);
});

test('curved closed stroke remains a centered band with a correctly wound hole', () => {
  const expanded = expandStrokeGeometry(stroked([circleContour('circle', 20)], { width: 2 }));
  assert.equal(expanded.contours.length, 2);
  assert.deepEqual(expanded.contours.map((item) => Math.sign(signedArea(item))).sort(), [-1, 1]);
  assert.ok(
    Math.abs(expanded.contours.reduce((sum, item) => sum + signedArea(item), 0) - 80 * Math.PI) <
      1.5,
  );
  const limits = bounds(expanded);
  assert.ok(Math.abs(limits[0] + 21) < 0.1);
  assert.ok(Math.abs(limits[1] + 21) < 0.1);
  assert.ok(Math.abs(limits[2] - 21) < 0.1);
  assert.ok(Math.abs(limits[3] - 21) < 0.1);
});

test('independent input groups retain separate namespaced output groups', () => {
  const source = stroked([
    contour(
      'first',
      [
        [0, 0],
        [30, 0],
        [30, 20],
        [0, 20],
      ],
      {
        closed: true,
        group: 'shape-a',
      },
    ),
    contour(
      'second',
      [
        [10, 0],
        [40, 0],
        [40, 20],
        [10, 20],
      ],
      {
        closed: true,
        group: 'shape-b',
      },
    ),
  ]);
  const expanded = expandStrokeGeometry(source, { prefix: 'independent' });
  assert.equal(expanded.contours.length, 4);
  assert.deepEqual(
    [...new Set(expanded.contours.map((item) => item.group))],
    ['independent:g0', 'independent:g1'],
  );
  for (const group of ['independent:g0', 'independent:g1']) {
    const member = expanded.contours.filter((item) => item.group === group);
    assert.equal(member.length, 2);
    assert.deepEqual(member.map((item) => Math.sign(signedArea(item))).sort(), [-1, 1]);
  }
});

test('curved contours remain partitioned by input group', () => {
  const second = circleContour('second-circle', 10, { group: 'second' });
  second.points = second.points.map((item) => ({
    ...item,
    position: [item.position[0] + 60, item.position[1]],
  }));
  const expanded = expandStrokeGeometry(
    stroked([circleContour('first-circle', 10, { group: 'first' }), second], { width: 2 }),
    { prefix: 'curved-groups' },
  );
  assert.deepEqual(
    [...new Set(expanded.contours.map((item) => item.group))],
    ['curved-groups:g0', 'curved-groups:g1'],
  );
  for (const group of ['curved-groups:g0', 'curved-groups:g1']) {
    const member = expanded.contours.filter((item) => item.group === group);
    assert.equal(member.length, 2);
    assert.deepEqual(member.map((item) => Math.sign(signedArea(item))).sort(), [-1, 1]);
  }
});

test('zero width returns empty geometry and all-zero dashes are treated as solid', () => {
  const line = [
    contour('line', [
      [0, 0],
      [20, 0],
    ]),
  ];
  assert.deepEqual(expandStrokeGeometry(stroked(line, { width: 0 })), { contours: [] });
  assert.deepEqual(
    expandStrokeGeometry(stroked(line, { dash: [0, 0, 0] })),
    expandStrokeGeometry(stroked(line)),
  );
});

test('rejects dashes, uncertifiable curves, missing stroke, invalid style, and too-thin strokes', () => {
  const line = [
    contour('line', [
      [0, 0],
      [20, 0],
    ]),
  ];
  assert.throws(() => expandStrokeGeometry(stroked(line, { dash: [2, 3] })), /solid strokes only/i);
  assert.throws(() => expandStrokeGeometry({ contours: line }), /Stroke node/i);
  const invalidHandles = [
    contour('bad', [
      [0, 0],
      [20, 0],
    ]),
  ];
  invalidHandles[0].points[0].handleA = [Infinity, 0];
  assert.throws(() => expandStrokeGeometry(stroked(invalidHandles)), /finite/i);
  const reversingCurve = [
    contour(
      'curve',
      [
        [0, 0],
        [20, 0],
      ],
      { handles: [[], [[1, 0]]] },
    ),
  ];
  assert.throws(() => expandStrokeGeometry(stroked(reversingCurve)), /flattening|curve/i);
  for (const stroke of [
    { width: -1 },
    { width: Infinity },
    { cap: 'triangle' },
    { join: 'arcs' },
    { miterLimit: 100.01 },
    { width: 0.0001 },
  ])
    assert.throws(() => expandStrokeGeometry(stroked(line, stroke)), /Expand Stroke|stroke width/i);
});

test('shared cubic centerline flattener respects the caller subdivision-work budget', () => {
  assert.throws(
    () =>
      flattenCubicCenterline(
        [
          { x: 0, y: 0 },
          { x: 0, y: 10 },
          { x: 10, y: 10 },
          { x: 10, y: 0 },
        ],
        { tolerance: 0.1, floatGuard: 0, maxVertices: 256, maxNodes: 1 },
      ),
    /subdivision work limit exceeded/i,
  );
});

test('enforces source, round-work, output, and coordinate budgets', () => {
  const line = [
    contour('line', [
      [0, 0],
      [20, 0],
    ]),
  ];
  assert.throws(
    () =>
      expandStrokeGeometry(
        stroked(Array.from({ length: 33 }, (_, index) => contour(`c${index}`, []))),
      ),
    /32-contour input budget/i,
  );
  const complexCurve = [
    contour(
      'complex',
      [
        [0, 0],
        [5000, 0],
      ],
      {
        handles: [
          [undefined, [500, 5000]],
          [[-500, -5000], undefined],
        ],
      },
    ),
  ];
  assert.throws(
    () => expandStrokeGeometry(stroked(complexCurve, { width: 2, join: 'bevel' })),
    /256-vertex|flattened centerline/i,
  );
  assert.throws(
    () =>
      expandStrokeGeometry(
        stroked([
          contour(
            'large',
            Array.from({ length: 257 }, (_, index) => [index, index % 2]),
          ),
        ]),
      ),
    /256-point input budget/i,
  );
  assert.throws(
    () =>
      expandStrokeGeometry(
        stroked(
          [
            contour('one', [
              [0, 0],
              [100, 0],
            ]),
            contour('two', [
              [0, 20],
              [100, 20],
            ]),
          ],
          { width: 100000, cap: 'round' },
        ),
      ),
    /4096-vertex work budget/i,
  );
  assert.throws(
    () =>
      expandStrokeGeometry(
        stroked(
          [
            contour('far', [
              [0, 0],
              [200000, 0],
            ]),
          ],
          { width: 100 },
        ),
      ),
    /safe integer envelope/i,
  );
});

test('rejects kernel output over the 2048-point budget', () => {
  const originalExecute = NectClipperOffset.prototype.execute;
  NectClipperOffset.prototype.execute = (_delta, solution) => {
    solution.push(Array.from({ length: 2049 }, (_, x) => ({ x, y: 0 })));
  };
  try {
    assert.throws(
      () =>
        expandStrokeGeometry(
          stroked([
            contour(
              'curve',
              [
                [0, 0],
                [20, 20],
              ],
              {
                handles: [
                  [undefined, [0, 10]],
                  [[-10, 0], undefined],
                ],
              },
            ),
          ]),
        ),
      /2048-point geometry budget/i,
    );
  } finally {
    NectClipperOffset.prototype.execute = originalExecute;
  }
});

test('does not mutate the source and deterministically namespaces output IDs', () => {
  const source = stroked([
    contour(
      'line',
      [
        [1, 2],
        [20, 2],
      ],
      { group: 'source-group' },
    ),
    contour(
      'closed',
      [
        [4, 4],
        [8, 4],
        [8, 8],
        [4, 8],
      ],
      {
        closed: true,
        group: 'source-group',
      },
    ),
    contour(
      'curve',
      [
        [50, 0],
        [70, 20],
      ],
      {
        group: 'source-group',
        handles: [
          [undefined, [0, 10]],
          [[-10, 0], undefined],
        ],
      },
    ),
  ]);
  const before = structuredClone(source);
  const options = { prefix: 'stable' };
  const first = expandStrokeGeometry(source, options);
  assert.deepEqual(source, before);
  assert.deepEqual(expandStrokeGeometry(source, options), first);
  assert.ok(first.contours.every((item) => item.group === 'stable:g0'));
  const contourIds = first.contours.map((item) => item.id);
  const pointIds = first.contours.flatMap((item) => item.points.map((p) => p.id));
  assert.equal(new Set(contourIds).size, contourIds.length);
  assert.equal(new Set(pointIds).size, pointIds.length);
  assert.ok(
    first.contours.every((item) =>
      item.points.every(
        (p) => p.handleA[0] === 0 && p.handleA[1] === 0 && p.handleB[0] === 0 && p.handleB[1] === 0,
      ),
    ),
  );
});
