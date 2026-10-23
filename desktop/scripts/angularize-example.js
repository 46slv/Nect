// Actual shared-core comparison of curve resampling into straight segments.
import fs from 'node:fs';
import opentype from 'opentype.js';
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
  bytes = fs.readFileSync(new URL('public/fonts/DejaVuSans.ttf', root));
const fonts = {
  default: opentype.parse(
    bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
  ),
};
const doc = document();
doc.name = 'Curve to angular segments';
doc.width = 1000;
doc.height = 270;
doc.layers = [];
function chain(name, nodes) {
  const l = layer(name);
  l.id = `angular-${doc.layers.length}`;
  nodes.forEach((n, i) => (n.id = `${l.id}:n${i}`));
  l.nodes = nodes;
  l.output = nodes.at(-1).id;
  nodes.slice(1).forEach((n, i) => connect(l, nodes[i].id, n.id, 'path'));
  doc.layers.push(l);
}
for (const [i, count] of [null, 24, 8, 4].entries()) {
  const cx = 125 + i * 250,
    shape = node('Ellipse'),
    stroke = node('Stroke'),
    out = node('Output');
  shape.params.center.value = [cx, 115];
  shape.params.size.value = [145, 145];
  stroke.params.width.value = 5;
  stroke.params.color.value = { hex: '#406c85', alpha: 1 };
  const nodes = [shape];
  if (count) {
    const r = node('Resample Path');
    r.params.count.value = count;
    nodes.push(r);
  }
  nodes.push(stroke, out);
  chain(count ? count + ' points' : 'Original curve', nodes);
  const t = node('Text'),
    fill = node('Fill'),
    textOut = node('Output');
  t.text = count ? count + ' points' : 'Original curve';
  t.params.position.value = [cx - 75, 235];
  t.params.size.value = 22;
  fill.params.color.value = { hex: '#26343d', alpha: 1 };
  chain(t.text, [t, fill, textOut]);
}
const saved = serialize(doc),
  restored = deserialize(saved);
for (const l of restored.layers) {
  const r = evaluate(l, fonts);
  if (r.error) throw Error(r.error);
}
fs.writeFileSync(new URL('examples/angularize.json', root), saved + '\n');
fs.writeFileSync(new URL('artifacts/angularize.svg', root), exportSVG(restored, fonts));
console.log('Verified 8 editable layers; actual SVG comparison saved.');
