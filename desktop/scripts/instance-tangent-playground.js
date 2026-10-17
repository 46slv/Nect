// Actual editable Instance on Points comparison, including tangent alignment.
import fs from 'node:fs';
import opentype from 'opentype.js';
import {
  document,
  layer,
  node,
  point,
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
doc.name = 'Instance Tangent Playground';
doc.width = 800;
doc.height = 360;
doc.layers = [];
function add(name, nodes, links) {
  const l = layer(name);
  l.id = `instance-demo-${doc.layers.length}`;
  l.nodes = nodes;
  nodes.forEach((n, i) => {
    n.id = `${l.id}:n${i}`;
    if (n.points)
      n.points.forEach((p, j) => {
        p.id = `${n.id}:p${j}`;
      });
    n.position = [i * 240, 64];
  });
  l.output = nodes.at(-1).id;
  for (const [a, b, input] of links) connect(l, nodes[a].id, nodes[b].id, input);
  doc.layers.push(l);
  return l;
}
function label(text, x, y, size = 14) {
  const t = node('Text'),
    f = node('Fill'),
    o = node('Output');
  t.text = text;
  t.params.position.value = [x, y];
  t.params.size.value = size;
  f.params.color.value = { hex: '#334550', alpha: 1 };
  add(
    text,
    [t, f, o],
    [
      [0, 1, 'path'],
      [1, 2, 'path'],
    ],
  );
}
const bg = node('Rectangle'),
  white = node('Fill'),
  out = node('Output');
bg.params.center.value = [400, 180];
bg.params.size.value = [800, 360];
white.params.color.value = { hex: '#ffffff', alpha: 1 };
add(
  'White paper',
  [bg, white, out],
  [
    [0, 1, 'path'],
    [1, 2, 'path'],
  ],
);
label('Instance on Points → Tangent', 32, 40, 25);
label('Same source arrow, same target curve; only orientation changes', 32, 67, 14);
for (const [index, align] of [false, true].entries()) {
  const x = 200 + index * 400,
    y = 205;
  const target = node('Curve');
  target.params.closed.value = true;
  target.points = Array.from({ length: 12 }, (_, i) => {
    const a = (i * Math.PI) / 6;
    return point(x + 125 * Math.cos(a), y + 84 * Math.sin(a));
  });
  const guide = structuredClone(target),
    stroke = node('Stroke'),
    go = node('Output');
  stroke.params.color.value = { hex: '#c7d2d9', alpha: 1 };
  stroke.params.width.value = 1;
  add(
    `Target guide ${index}`,
    [guide, stroke, go],
    [
      [0, 1, 'path'],
      [1, 2, 'path'],
    ],
  );
  const source = node('Imported Path');
  source.rawPath = 'M-12 -4H3V-9L15 0L3 9V4H-12Z';
  const inst = node('Instance on Points');
  if (!inst.params.alignTangent) throw Error('Align Tangent integration is not ready');
  inst.params.alignTangent.value = align;
  const fill = node('Fill'),
    output = node('Output');
  fill.params.color.value = { hex: align ? '#126a77' : '#283a47', alpha: 1 };
  add(
    align ? 'Follow tangent' : 'Translation only',
    [source, target, inst, fill, output],
    [
      [0, 2, 'path'],
      [1, 2, 'points'],
      [2, 3, 'path'],
      [3, 4, 'path'],
    ],
  );
  label(align ? '02  ALIGN TANGENT' : '01  TRANSLATION ONLY', x - 150, 108);
  label(
    align
      ? 'Source +X follows the local curve direction'
      : 'Every copy keeps its original orientation',
    x - 150,
    330,
    12,
  );
}
const saved = serialize(doc),
  restored = deserialize(saved);
for (let i = 0; i < doc.layers.length; i++) {
  const a = evaluate(doc.layers[i], fonts),
    b = evaluate(restored.layers[i], fonts);
  if (a.error || b.error) throw Error(`${doc.layers[i].name}: ${a.error || b.error}`);
  if (JSON.stringify(a.path) !== JSON.stringify(b.path))
    throw Error('Reopen changed evaluated geometry');
}
fs.writeFileSync(new URL('examples/instance-tangent-playground.json', root), saved + '\n');
fs.writeFileSync(
  new URL('artifacts/instance-tangent-playground.svg', root),
  exportSVG(restored, fonts),
);
console.log(`Verified ${doc.layers.length} editable layers and exact reopened output.`);
