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
  assert.throws(() => run(1, { ...a, stroke: { width: 2 } }), /without stroke/);
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
