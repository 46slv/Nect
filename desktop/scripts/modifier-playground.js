// Rebuild a real editable Nect example and its SVG proof from the shared core.
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
  exportSVG,
  evaluate,
} from '../src/core.js';
const root = new URL('../', import.meta.url);
const bytes = fs.readFileSync(new URL('public/fonts/DejaVuSans.ttf', root));
const fonts = {
  default: opentype.parse(
    bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
  ),
};
const doc = document();
doc.name = 'Modifier Playground';
doc.width = 1080;
doc.height = 900;
doc.layers = [];
function chain(name, nodes, color = '#40586b', paint = true) {
  const l = layer(name);
  l.nodes = nodes;
  if (paint) {
    const fill = node('Fill');
    fill.params.color.value = { hex: color, alpha: 1 };
    l.nodes.push(fill);
  }
  const out = node('Output');
  l.nodes.push(out);
  const sample = doc.layers.length;
  l.id = `sample-layer-${sample}`;
  l.nodes.forEach((n, i) => {
    n.id = `sample-node-${sample}-${i}`;
    n.points.forEach((p, j) => {
      p.id = `sample-point-${sample}-${i}-${j}`;
    });
  });
  l.output = out.id;
  l.nodes.forEach((n, i) => {
    n.position = [40 + i * 300, 60];
    if (i) connect(l, l.nodes[i - 1].id, n.id, 'path');
  });
  doc.layers.push(l);
  return l;
}
function label(text, x, y, size = 18) {
  const n = node('Text');
  n.text = text;
  n.params.position.value = [x, y];
  n.params.size.value = size;
  chain(text, [n], '#26343d');
}
function circle(x, y) {
  const n = node('Ellipse');
  n.params.center.value = [x, y];
  n.params.size.value = [140, 110];
  return n;
}
function modifier(type, settings = {}) {
  const n = node(type);
  for (const [key, value] of Object.entries(settings)) n.params[key].value = value;
  return n;
}
label('NECT / live vector modifiers', 55, 62, 30);
label('Editable source graphs. Development build required.', 55, 94, 16);
const x = [190, 540, 890],
  y = [230, 490, 750];
let curve = node('Curve');
curve.points = [point(115, 175), point(175, 290), point(120, 245)];
chain('Mirror live pair', [curve, modifier('Mirror', { center: [190, 230], keepOriginal: true })]);
label('01  Mirror / live pair', 65, 345);
let star = node('Star');
star.params.center.value = [540, 230];
star.params.outerRadius.value = 78;
star.params.innerRadius.value = 39;
chain('Corner Round star', [star, modifier('Corner Round', { radius: 11 })], '#b05e3d');
label('02  Corner Round', 415, 345);
chain(
  'Resample ten points',
  [circle(890, 230), modifier('Resample Path', { count: 10 })],
  '#4b746d',
);
label('03  Resample Path', 765, 345);
chain('Seeded Roughen', [
  circle(190, 490),
  modifier('Resample Path', { count: 64 }),
  modifier('Roughen', { amount: 7, seed: 77 }),
]);
label('04  Roughen', 65, 605);
chain(
  'Smoothed rough outline',
  [
    circle(540, 490),
    modifier('Resample Path', { count: 64 }),
    modifier('Roughen', { amount: 12, seed: 77 }),
    modifier('Smooth Path', { factor: 0.5, iterations: 4 }),
  ],
  '#b05e3d',
);
label('05  Smooth Path', 415, 605);
const stroke = node('Stroke');
stroke.params.width.value = 7;
stroke.params.color.value = { hex: '#4b746d', alpha: 1 };
stroke.strokeCap = 'round';
chain(
  'Trimmed curve',
  [circle(890, 490), modifier('Trim Path', { start: 0.08, end: 0.78 }), stroke],
  '#4b746d',
  false,
);
label('06  Trim Path', 765, 605);
curve = node('Curve');
curve.params.closed.value = false;
curve.points = [point(90, 800), point(140, 710), point(200, 785), point(270, 710)];
const outlineStroke = node('Stroke');
outlineStroke.params.width.value = 18;
outlineStroke.strokeCap = 'round';
outlineStroke.strokeJoin = 'round';
chain('Geometric stroke outline', [curve, outlineStroke, node('Expand Stroke')], '#b05e3d');
label('07  Expand Stroke', 65, 865);
label('Try changing source shapes, amounts, seeds and order.', 415, 735, 17);
label('Each layer is a separate editable node chain.', 415, 767, 17);
const saved = serialize(doc),
  restored = deserialize(saved);
for (const l of restored.layers) {
  const r = evaluate(l, fonts);
  if (r.error) throw Error(`${l.name}: ${r.error}`);
}
fs.writeFileSync(new URL('examples/modifier-playground.json', root), saved + '\n');
fs.mkdirSync(new URL('artifacts/', root), { recursive: true });
fs.writeFileSync(new URL('artifacts/modifier-playground.svg', root), exportSVG(restored, fonts));
console.log(`Verified ${doc.layers.length} editable layers; wrote example JSON and SVG.`);
