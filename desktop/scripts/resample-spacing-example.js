// Editable Count/Spacing comparison. Select each Path Remap to inspect sampled anchors.
import fs from 'node:fs';
import {
  document,
  layer,
  node,
  connect,
  serialize,
  deserialize,
  evaluate,
  exportSVG,
} from '../src/core.js';
const root = new URL('../', import.meta.url),
  doc = document();
doc.name = 'Resample Count and Maximum Spacing';
doc.schemaVersion = 12;
doc.width = 680;
doc.height = 220;
doc.layers = [];
for (const [x, mode] of [
  [40, 'count'],
  [360, 'spacing'],
]) {
  const l = layer(mode === 'count' ? 'Count 6: both contours' : 'Maximum spacing 24 px'),
    source = node('Imported Path'),
    resample = node('Resample Path'),
    remap = node('Path Remap'),
    stroke = node('Stroke'),
    out = node('Output');
  l.id = `resample-example-${mode}`;
  l.nodes = [source, resample, remap, stroke, out];
  l.nodes.forEach((n, i) => (n.id = `${l.id}:n${i}`));
  l.output = out.id;
  source.rawPath = `M${x} 70h120 M${x} 140h240`;
  resample.operation = mode;
  resample.params.count.value = 6;
  resample.params.spacing.value = 24;
  stroke.params.width.value = 4;
  stroke.params.color.value = { hex: '#406c85', alpha: 1 };
  l.nodes.slice(1).forEach((n, i) => connect(l, l.nodes[i].id, n.id, 'path'));
  doc.layers.push(l);
}
const saved = serialize(doc),
  restored = deserialize(saved);
for (const l of restored.layers) {
  const result = evaluate(l);
  if (result.error) throw Error(result.error);
  const counts = result.run(l.nodes[1].id).contours.map((c) => c.points.length);
  const expected = l.id.endsWith('count') ? [6, 6] : [6, 11];
  if (JSON.stringify(counts) !== JSON.stringify(expected)) throw Error('Unexpected sample counts');
  console.log(l.name + ': ' + counts.join(', ') + ' samples');
}
fs.writeFileSync(new URL('examples/resample-spacing.json', root), saved + '\n');
fs.mkdirSync(new URL('artifacts/', root), { recursive: true });
fs.writeFileSync(new URL('artifacts/resample-spacing.svg', root), exportSVG(restored));
