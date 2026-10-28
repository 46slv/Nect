import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, node, connect, exportScene } from '../src/core.js';
import { colorCoveragePasses as passes } from '../src/color-coverage-passes.js';
function fixture() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    shape = node('Rectangle'),
    fill = node('Fill'),
    stroke = node('Stroke'),
    output = layer.nodes[0];
  layer.nodes = [shape, fill, stroke, output];
  connect(layer, shape.id, fill.id, 'path');
  connect(layer, fill.id, stroke.id, 'path');
  connect(layer, stroke.id, output.id, 'path');
  return { doc, layer, scene: exportScene(doc) };
}
test('actual evaluated renderer emits fill then stroke without mutating artwork', () => {
  const { doc, layer, scene } = fixture(),
    before = JSON.stringify(scene);
  const result = passes(scene, doc, new Map([[layer.id, { fill: 'c1', stroke: 'c2' }]]));
  assert.deepEqual(
    result.map((p) => [p.paint, p.slotId]),
    [
      ['fill', 'c1'],
      ['stroke', 'c2'],
    ],
  );
  assert.match(result[0].svg, /<path/);
  assert.doesNotMatch(result[0].svg, / stroke=/);
  assert.match(result[1].svg, /fill="none"/);
  assert.match(result[1].svg, / stroke=/);
  assert.match(result[0].svg, /overflow="hidden"/);
  assert.equal(JSON.stringify(scene), before);
});
test('unbound paints are Other even when their display colors match', () => {
  const { doc, scene } = fixture();
  assert.deepEqual(
    passes(scene, doc).map((p) => p.slotId),
    [null, null],
  );
});
test('unsupported geometry rejects whole measurement rather than partial coverage', () => {
  const { doc, scene } = fixture();
  for (const patch of [
    { children: [] },
    { mask: {} },
    { kind: 'image' },
    { fill: { kind: 'linear-gradient' } },
    { contours: [{ group: 1 }] },
  ])
    assert.throws(
      () => passes([...scene, { ...scene[0], value: { ...scene[0].value, ...patch } }], doc),
      /plain solid paths/,
    );
  assert.throws(() => passes(Array(129).fill(scene[0]), doc), /budget/);
  assert.throws(() => passes(scene, { width: NaN, height: 1 }), /bounds/);
});

test('scene order and paint alpha survive the production renderer bridge', () => {
  const a = fixture(),
    b = fixture();
  a.scene[0].value.fill.alpha = 0.25;
  a.scene[0].value.stroke.alpha = 0.5;
  const result = passes([...a.scene, ...b.scene], a.doc);
  assert.deepEqual(
    result.map((p) => p.layerId),
    [a.layer.id, a.layer.id, b.layer.id, b.layer.id],
  );
  assert.match(result[0].svg, /fill-opacity="0.25"/);
  assert.match(result[1].svg, /stroke-opacity="0.5"/);
  const reverse = passes([...b.scene, ...a.scene], a.doc);
  assert.equal(reverse[0].layerId, b.layer.id);
});

test('repeater appearance is never flattened into independent child alpha', () => {
  const { doc, scene } = fixture();
  for (const field of ['repeaterOpacity', 'repeaterFill', 'repeaterStroke']) {
    const value = structuredClone(scene[0].value);
    value.contours[0][field] = field === 'repeaterOpacity' ? 0.5 : { hex: '#ffffff', alpha: 1 };
    assert.throws(() => passes([{ ...scene[0], value }], doc), /plain solid paths/);
  }
});
