// Reproducible demonstration: explicit groups remain independently offset and painted.
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
doc.width = 640;
doc.height = 300;
doc.name = 'Grouped Offset';
doc.layers = [];
function outputLayer(name, nodes) {
  const l = layer(name);
  l.id = `group-example-${doc.layers.length}`;
  nodes.forEach((n, i) => (n.id = `${l.id}:n${i}`));
  l.nodes = nodes;
  l.output = nodes.at(-1).id;
  doc.layers.push(l);
  return l;
}
for (const [x, amount, title] of [
  [130, 0, 'Joined groups'],
  [440, 10, 'Offset +10 px'],
]) {
  const a = node('Ellipse'),
    b = node('Ellipse'),
    join = node('Join Geometry'),
    offset = node('Closed Path Offset'),
    fill = node('Fill'),
    out = node('Output');
  for (const [n, cx] of [
    [a, x],
    [b, x + 65],
  ]) {
    n.params.center.value = [cx, 140];
    n.params.size.value = [125, 125];
  }
  offset.params.amount.value = amount;
  offset.operation = 'miter';
  fill.params.color.value = { hex: '#406c85', alpha: 0.45 };
  const l = outputLayer(title, [a, b, join, offset, fill, out]);
  connect(l, a.id, join.id, 'a');
  connect(l, b.id, join.id, 'b');
  connect(l, join.id, offset.id, 'path');
  connect(l, offset.id, fill.id, 'path');
  connect(l, fill.id, out.id, 'path');
  const text = node('Text'),
    tf = node('Fill'),
    to = node('Output');
  text.text = title;
  text.params.position.value = [x - 65, 265];
  text.params.size.value = 22;
  tf.params.color.value = { hex: '#26343d', alpha: 1 };
  const label = outputLayer(title + ' label', [text, tf, to]);
  connect(label, text.id, tf.id, 'path');
  connect(label, tf.id, to.id, 'path');
}
const saved = serialize(doc),
  restored = deserialize(saved);
for (const l of restored.layers) {
  const r = evaluate(l, fonts);
  if (r.error) throw Error(l.name + ': ' + r.error);
}
fs.writeFileSync(new URL('examples/grouped-offset.json', root), saved + '\n');
fs.mkdirSync(new URL('artifacts/', root), { recursive: true });
fs.writeFileSync(new URL('artifacts/grouped-offset.svg', root), exportSVG(restored, fonts));
console.log('Verified four editable layers and grouped offset SVG.');
