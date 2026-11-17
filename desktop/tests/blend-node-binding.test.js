import test from 'node:test';
import assert from 'node:assert/strict';
import { evaluateBlend } from '../src/blend-node-binding.js';
import { paintedGroup } from '../src/composition-reference.js';
const shape = (x, color) => ({
  fill: { hex: color, alpha: 1 },
  contours: [
    {
      closed: true,
      points: [
        [x, 0],
        [x + 4, 0],
        [x + 4, 4],
        [x, 4],
      ].map((position, i) => ({ id: `p${i}`, position, handleA: [0, 0], handleB: [0, 0] })),
    },
  ],
});
const a = shape(0, '#000000'),
  b = shape(8, '#ffffff');
const run = (steps, first = a, second = b) =>
  evaluateBlend('', (id) => ({ a: first, b: second, steps })[id], {
    nodeId: 'blend1',
    groupAppearance: paintedGroup,
  });
test('steps create intermediate geometry and linear-light color without source mutation', () => {
  const before = JSON.stringify([a, b]);
  const out = run(1);
  assert.equal(out.children.length, 3);
  assert.deepEqual(out.children[1].contours[0].points[0].position, [4, 0]);
  assert.equal(out.children[1].fill.hex, '#bcbcbc');
  assert.equal(JSON.stringify([a, b]), before);
  assert.equal(run(0).children.length, 2);
  assert.equal(run(100).children.length, 102);
});
test('rejects unsupported paint and invalid steps', () => {
  assert.throws(() => run(255), /steps/);
  assert.throws(() => run(1, { ...a, stroke: { width: 2 } }), /matching stroke/);
});
test('preserves export blockers and rejects per-copy paint or oversized result', () => {
  assert.deepEqual(run(1, { ...a, exportBlockers: ['pending'] }).exportBlockers, ['pending']);
  assert.throws(() => run(1, { ...a, repeaterOpacity: [0.2] }), /per-copy/);
  const many = shape(0, '#000000');
  many.contours[0].points = Array.from({ length: 512 }, (_, i) => ({
    id: `p${i}`,
    position: [i, 0],
    handleA: [0, 0],
    handleB: [0, 0],
  }));
  assert.throws(() => run(254, many, many), /geometry budget/);
});
test('refuses per-contour overrides and authored character metadata', () => {
  const overridden = structuredClone(a);
  overridden.contours[0].repeaterOpacity = 0;
  assert.throws(() => run(1, overridden), /per-copy/);
  assert.throws(() => run(1, { ...a, characterOccurrences: [{ suspended: true }] }), /per-copy/);
});
test('interpolates compatible solid strokes', () => {
  const sa = { hex: '#000000', alpha: 1, width: 2, cap: 'round', join: 'round' },
    sb = { ...sa, hex: '#ffffff', width: 6 };
  const result = run(1, { ...a, stroke: sa }, { ...b, stroke: sb });
  assert.equal(result.children[1].stroke.width, 4);
  assert.equal(result.children[1].stroke.hex, '#bcbcbc');
  assert.throws(
    () => run(1, { ...a, stroke: sa }, { ...b, stroke: { ...sb, cap: 'butt' } }),
    /compatible/,
  );
});
test('rejects dashed, non-solid and oversized strokes', () => {
  const base = {
    hex: '#000000',
    alpha: 1,
    width: 2,
    cap: 'round',
    join: 'round',
    dash: [],
    miterLimit: 4,
    dashOffset: 0,
  };
  for (const stroke of [
    { ...base, dash: [2, 5] },
    { ...base, kind: 'linear-gradient' },
    { ...base, width: 100001 },
  ])
    assert.throws(() => run(1, { ...a, stroke }, { ...b, stroke }), /compatible/);
  const reordered = {
    width: 2,
    alpha: 1,
    hex: '#000000',
    join: 'round',
    cap: 'round',
    dash: [],
    dashOffset: 0,
    miterLimit: 4,
  };
  assert.equal(run(1, { ...a, stroke: base }, { ...b, stroke: reordered }).children.length, 3);
});

test('stroke-only blending preserves absent fill and validates appearance compatibility', () => {
  const stroke = { hex: '#000000', alpha: 1, width: 2 };
  const first = { ...a, fill: null, stroke };
  const second = { ...b, fill: null, stroke: { ...stroke, width: 6, alpha: 0.5 } };
  const before = JSON.stringify([first, second]);
  const result = run(1, first, second);
  assert.ok(result.children.every((child) => child.fill == null));
  assert.equal(result.children[1].stroke.width, 4);
  assert.equal(result.children[1].stroke.alpha, 0.75);
  assert.equal(JSON.stringify([first, second]), before);
  assert.throws(() => run(1, first, { ...second, fill: b.fill }), /matching fill/);
  assert.throws(() => run(1, { ...first, stroke: null }, { ...second, stroke: null }), /paint/);
});
