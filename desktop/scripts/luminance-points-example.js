import { mkdirSync, writeFileSync } from 'node:fs';
import { document, node, serialize, deserialize, exportSVG, evaluate } from '../src/core.js';
import assert from 'node:assert/strict';

const doc = document();
doc.name = 'Luminance points and arbitrary motifs';
doc.width = 304;
doc.height = 288;
doc.layers = [];
for (const [index, type] of ['Star', 'Polygon'].entries()) {
  const layer = { ...document().layers[0], id: `layer-luminance-points-${index}`, name: type };
  const source = node('Rectangle'),
    paint = node('Linear Gradient Fill'),
    points = node('Luminance to Points'),
    motif = node(type),
    instances = node('Instance on Points'),
    fill = node('Fill'),
    out = node('Output');
  const x = 16 + 144 * index;
  source.params.center.value = [x + 64, 144];
  source.params.size.value = [128, 256];
  paint.params.path.source = source.id;
  paint.params.start.value = [x, 16];
  paint.params.end.value = [x, 272];
  paint.params.startColor.value = { hex: type === 'Star' ? '#ffffff' : '#000000', alpha: 1 };
  paint.params.endColor.value = { hex: type === 'Star' ? '#000000' : '#ffffff', alpha: 1 };
  points.params.path.source = paint.id;
  points.params.pitch.value = 16;
  motif.params.center.value = [0, 0];
  if (type === 'Star') {
    motif.params.points.value = 4;
    motif.params.outerRadius.value = 16;
    motif.params.innerRadius.value = 2.88;
  } else {
    motif.params.sides.value = 4;
    motif.params.radius.value = 14.142135623730951;
  }
  instances.params.path.source = motif.id;
  instances.params.points.source = points.id;
  fill.params.path.source = instances.id;
  fill.params.color.value = { hex: '#000000', alpha: 1 };
  out.params.path.source = fill.id;
  layer.nodes = [source, paint, points, motif, instances, fill, out];
  layer.output = out.id;
  layer.nodes.forEach((n, i) => {
    n.position = [40 + i * 300, 40];
    n.collapsed = n !== points;
  });
  doc.layers.push(layer);
}
const saved = serialize(deserialize(serialize(doc))),
  reopened = deserialize(saved),
  svg = exportSVG(reopened);
assert.equal(exportSVG(deserialize(saved)), svg);
for (const layer of reopened.layers) assert.ok(!evaluate(layer).error);
mkdirSync('test-results/dither-controls', { recursive: true });
writeFileSync('test-results/dither-controls/points-comparison.json', saved);
writeFileSync('test-results/dither-controls/points-comparison.svg', svg);
console.log('Separated Luminance to Points + native Star/Polygon + Instance on Points exported.');
