import test from 'node:test';
import assert from 'node:assert/strict';
import {
  constrainDirectionDrag,
  snapDirectionTranslation,
  snapGraphDirection,
  directionGuidePath,
} from '../src/direction-snapping.js';
import { snapTranslation, guideDefaults } from '../src/guides.js';
import { inverseAffineDelta } from '../src/direct-selection.js';
import { rotationFromHandle } from '../src/transform-handles.js';

const near = (a, b) => a.forEach((v, i) => assert.ok(Math.abs(v - b[i]) < 1e-8, `${a} != ${b}`));
const doc = () => ({
  width: 1000,
  height: 800,
  editor: { ...guideDefaults(), snap: true, snapGrid: false, snapArtboard: false },
});
const solve = (d, delta, options = {}, extras = []) =>
  snapDirectionTranslation(
    (sources, value, scale) =>
      snapTranslation(d, sources, value, scale, !options.enabled, null, [], [], extras),
    [[100], [100]],
    delta,
    { enabled: true, ...options },
  );

test('Shift selects each nearest 45 degree line, freezes it, releases and reselects', () => {
  for (const [input, expected] of [
    [
      [40, 5],
      [40, 0],
    ],
    [
      [5, 40],
      [0, 40],
    ],
    [
      [40, 36],
      [38, 38],
    ],
    [
      [-40, 36],
      [-38, 38],
    ],
    [
      [-40, -36],
      [-38, -38],
    ],
    [
      [40, -36],
      [38, -38],
    ],
  ]) {
    const state = {};
    assert.equal(constrainDirectionDrag(state, input, [1, 1], true).pending, true);
    near(constrainDirectionDrag(state, input, [4, 4], true).delta, expected);
    const direction = [...state.direction];
    constrainDirectionDrag(state, [99, -1], [99, -1], true);
    assert.deepEqual(state.direction, direction);
    near(constrainDirectionDrag(state, input, input, false).delta, input);
    assert.equal(state.direction, null);
    assert.deepEqual(constrainDirectionDrag(state, [3, -3], [3, -3], true).direction, [1, -1]);
  }
});

test('zoom changes only the activation tolerance, and projection returns exactly to origin', () => {
  for (const scale of [0.05, 0.125, 1, 8]) {
    const s = {};
    near(constrainDirectionDrag(s, [60, 40], [60 * scale, 40 * scale], true).delta, [50, 50]);
    near(constrainDirectionDrag(s, [0, 0], [0, 0], true).delta, [0, 0]);
  }
});

test('diagonal target snap keeps the direction and only reports guides actually reached', () => {
  const d = doc();
  d.editor.guides = [
    { id: 'gx', axis: 'x', position: 140 },
    { id: 'gy', axis: 'y', position: 144 },
  ];
  const result = solve(d, [38, 38], { direction: [1, 1], enabled: true, origin: [100, 100] });
  near(result.delta, [40, 40]);
  assert.deepEqual(
    result.matches.filter((m) => m.kind === 'guide').map((m) => m.position),
    [140],
  );
  assert.equal(directionGuidePath(result.matches.at(-1)), 'M100 100L140 140');
});

test('diagonal tolerance is six CSS pixels along the constrained line', () => {
  for (const scale of [0.125, 1, 8]) {
    const d = doc();
    d.editor.guides = [{ id: 'gx', axis: 'x', position: 140 }];
    const inside = 40 - 5.9 / (scale * Math.SQRT2);
    near(solve(d, [inside, inside], { direction: [1, 1], scale, enabled: true }).delta, [40, 40]);
    const outside = 40 - 6.1 / (scale * Math.SQRT2);
    near(solve(d, [outside, outside], { direction: [1, 1], scale, enabled: true }).delta, [
      outside,
      outside,
    ]);
  }
});

test('explicit targets precede grid, with stable exact target ties', () => {
  const d = doc();
  d.editor.snapGrid = true;
  d.editor.gridSize = 10;
  d.editor.guides = [
    { id: 'x', axis: 'x', position: 142 },
    { id: 'y', axis: 'y', position: 138 },
  ];
  const r = solve(d, [40, 40], { direction: [1, 1], enabled: true });
  near(r.delta, [42, 42]);
  assert.equal(r.matches[0].kind, 'guide');
  assert.equal(r.matches[0].axis, 0);
});

test('grid and artboard are reached on a diagonal without inventing diagonal saved guides', () => {
  const d = doc();
  d.editor.snapGrid = true;
  d.editor.gridAnchor = 'center';
  near(solve(d, [39, 39], { direction: [1, 1], enabled: true }).delta, [40, 40]);
  d.editor.snapGrid = false;
  d.editor.snapArtboard = true;
  near(solve(d, [398, 398], { direction: [1, 1], enabled: true }).delta, [400, 400]);
  assert.deepEqual(d.editor.guides, []);
});

test('automatic 45 degree fallback yields to XY targets, bypass and Snap OFF', () => {
  const d = doc();
  near(solve(d, [30, 34], { enabled: true }).delta, [32, 32]);
  near(solve(d, [30, 34], { enabled: false }).delta, [30, 34]);
  d.editor.snap = false;
  // Caller supplies the existing master toggle to the direction fallback.
  near(solve(d, [30, 34], { enabled: d.editor.snap }).delta, [30, 34]);
  d.editor.snap = true;
  d.editor.guides = [{ id: 'x', axis: 'x', position: 130 }];
  near(solve(d, [30, 34], { enabled: true }).delta, [30, 34]);
  d.editor.guides = [];
  near(solve(d, [4, 0], { enabled: true }).delta, [4, 0]);
});

test('Shift constraint works with bypass and Snap OFF; a one-axis guide never gains another axis', () => {
  const d = doc();
  near(solve(d, [31, 29], { direction: [1, 1], enabled: false }).delta, [30, 30]);
  d.editor.snap = false;
  near(solve(d, [31, 29], { direction: [1, 1], enabled: false }).delta, [30, 30]);
  const r = snapDirectionTranslation(
    (s, v, z) => snapTranslation(doc(), s, v, z),
    [[100], []],
    [30, 34],
    { enabled: true },
  );
  near(r.delta, [30, 34]);
});

test('handle direction is measured from its vertex, not its original handle endpoint', () => {
  const r = solve(doc(), [10, 12], {
    direction: [1, 1],
    enabled: true,
    offset: [20, 0],
    origin: [80, 100],
  });
  near(r.delta, [1, 21]);
  assert.equal(directionGuidePath(r.matches.at(-1)), 'M80 100L101 121');
});

test('display-space direction survives reflected, rotated and nonuniform parent transforms', () => {
  for (const m of [
    [0, -2, -3, 0, 100, 50],
    [-2, 1, 0.5, 3, 100, 50],
  ]) {
    const display = constrainDirectionDrag({}, [42, 38], [84, 76], true).delta;
    const local = inverseAffineDelta(m, display);
    near([m[0] * local[0] + m[2] * local[1], m[1] * local[0] + m[3] * local[1]], [40, 40]);
  }
});

test('character gap cross feedback uses final constrained translation', () => {
  const r = solve(doc(), [39, 39], { direction: [1, 1], enabled: true }, [
    {
      axis: 0,
      kind: 'character-gap',
      translation: 40,
      position: 140,
      fixedCross: 110,
      movingCross: 100,
    },
  ]);
  near(r.delta, [40, 40]);
  assert.equal(r.matches[0].cross, 140);
});

const moving = [{ id: 'a', x: 100, y: 100, width: 50, height: 50 }];
test('graph node diagonal targets retain group offsets, clamp at origin and bypass candidates', () => {
  const targets = [...moving, { id: 'b', x: 190, y: 300, width: 50, height: 50 }];
  const r = snapGraphDirection(moving, targets, [39, 37], 1, false, 6, { shift: true });
  near(r.delta, [40, 40]);
  const bypass = snapGraphDirection(moving, targets, [39, 37], 1, true, 6, { shift: true });
  near(bypass.delta, [38, 38]);
  assert.deepEqual(bypass.guides, []);
  near(
    snapGraphDirection(moving, targets, [-150, -130], 1, false, 6, { shift: true }).delta,
    [-100, -100],
  );
  const reflected = [{ ...moving[0], x: 50 }];
  near(
    snapGraphDirection(reflected, [], [-90, 100], 1, false, 6, { shift: true }).delta,
    [-50, 50],
  );
});

test('rotation already includes all 45 degree multiples in its existing Shift 15 degree constraint', () => {
  for (const a of [-315, -45, 45, 135, 315, 405]) assert.equal(rotationFromHandle(0, a, true), a);
});
