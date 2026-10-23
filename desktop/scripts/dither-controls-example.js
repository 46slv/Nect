import { mkdirSync, writeFileSync } from 'node:fs';
import { document, node, serialize, deserialize, exportSVG, evaluate } from '../src/core.js';
import assert from 'node:assert/strict';

// Synthetic gradients only. User reference images are not copied into Git.
const doc = document();
doc.name = 'Dither shape and overlap comparison';
doc.width = 304;
doc.height = 288;
doc.layers = [];
for (const [index, kind] of ['star', 'square'].entries()) {
  const layer = { ...document().layers[0], id: `layer-dither-${index}`, name: kind };
  const shape = node('Rectangle'),
    fill = node('Linear Gradient Fill'),
    effect = node('Halftone'),
    out = node('Output');
  const x = 16 + index * 144;
  shape.params.center.value = [x + 64, 144];
  shape.params.size.value = [128, 256];
  fill.params.path.source = shape.id;
  fill.params.start.value = [x, 16];
  fill.params.end.value = [x, 272];
  fill.params.startColor.value = { hex: kind === 'star' ? '#ffffff' : '#000000', alpha: 1 };
  fill.params.endColor.value = { hex: kind === 'star' ? '#000000' : '#ffffff', alpha: 1 };
  effect.operation = kind;
  effect.params.path.source = fill.id;
  effect.params.pitch.value = 16;
  effect.params.dotSize.value = kind === 'star' ? 28 : 20;
  effect.params.dotRotation.value = kind === 'square' ? 45 : 0;
  effect.params.starInnerRatio.value = 0.18;
  out.params.path.source = effect.id;
  layer.nodes = [shape, fill, effect, out];
  layer.nodes.forEach((node, index) => {
    node.position = [40 + index * 300, 40];
    node.collapsed = node !== effect;
  });
  layer.output = out.id;
  doc.layers.push(layer);
}
const saved = serialize(deserialize(serialize(doc))),
  reopened = deserialize(saved),
  svg = exportSVG(reopened);
assert.equal(exportSVG(deserialize(saved)), svg);
for (const layer of reopened.layers) assert.ok(!evaluate(layer).error);
mkdirSync('test-results/dither-controls', { recursive: true });
writeFileSync('test-results/dither-controls/comparison.json', saved);
writeFileSync('test-results/dither-controls/comparison.svg', svg);
console.log('Synthetic star and overlapping diamond gradients exported and cold-reopened.');
