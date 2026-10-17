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
doc.height = 1940;
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
chain(
  'Simplified sampled curve',
  [
    circle(540, 750),
    modifier('Resample Path', { count: 64 }),
    modifier('Simplify Path', { tolerance: 4 }),
  ],
  '#40586b',
);
label('08  Simplify Path', 415, 865);
const dashStroke = node('Stroke');
dashStroke.params.width.value = 7;
dashStroke.params.color.value = { hex: '#4b746d', alpha: 1 };
dashStroke.strokeCap = 'round';
chain(
  'Real dash geometry',
  [
    circle(890, 750),
    modifier('Dash Path', { dashLength: 16, gapLength: 12, offset: 4 }),
    dashStroke,
  ],
  '#4b746d',
  false,
);
label('09  Dash Path', 765, 865);
const cubic = node('Imported Path');
cubic.rawPath = 'M95 1070 C75 940 300 940 270 1070';
const curveStroke = node('Stroke');
curveStroke.params.width.value = 6;
curveStroke.params.color.value = { hex: '#40586b', alpha: 1 };
chain(
  'Shape-preserving subdivisions',
  [cubic, modifier('Subdivide Path', { segments: 4 }), curveStroke],
  '#40586b',
  false,
);
label('10  Subdivide Path', 65, 1125);
const bloated = node('Rectangle');
bloated.params.center.value = [540, 1010];
bloated.params.size.value = [105, 105];
chain('Bloat +70%', [bloated, modifier('Pucker & Bloat', { amount: 70 })], '#b05e3d');
label('11  Bloat (+70%)', 415, 1125);
const waveLine = node('Imported Path');
waveLine.rawPath = 'M730 1010 L1050 1010';
const wave = modifier('Wave Points', {
  amplitude: 28,
  wavelength: 100,
  phase: 0,
  keepEndpoints: true,
});
wave.operation = 'vertical';
const waveStroke = node('Stroke');
waveStroke.params.width.value = 7;
waveStroke.params.color.value = { hex: '#4b746d', alpha: 1 };
chain(
  'Waved sampled line',
  [waveLine, modifier('Resample Path', { count: 49 }), wave, waveStroke],
  '#4b746d',
  false,
);
label('12  Wave Points', 765, 1125);
// Direction and seam controls are easiest to see against the same full curve.
for (const [cx, title, effect] of [
  [180, 'Trim reference', null],
  [540, '13  Reverse Path', node('Reverse Path')],
  [890, '14  Set Path Start', modifier('Set Path Start', { start: 0.25 })],
]) {
  const referenceStroke = node('Stroke');
  referenceStroke.params.width.value = 2;
  referenceStroke.params.color.value = { hex: '#c9d0d3', alpha: 1 };
  chain(`${title} reference`, [circle(cx, 1270), referenceStroke], '#c9d0d3', false);
  const activeStroke = node('Stroke');
  activeStroke.params.width.value = 8;
  activeStroke.params.color.value = { hex: '#40586b', alpha: 1 };
  activeStroke.strokeCap = 'round';
  chain(
    title,
    [
      circle(cx, 1270),
      ...(effect ? [effect] : []),
      modifier('Trim Path', { end: 0.5 }),
      activeStroke,
    ],
    '#40586b',
    false,
  );
  label(title, cx - 125, 1385);
}
for (const [cx, title, effect] of [
  [180, 'Text reference', null],
  [540, '15  Taper', modifier('Taper', { amount: 50 })],
  [890, '16  Bend Points', modifier('Bend Points', { angle: 120 })],
]) {
  const text = node('Text');
  text.text = 'NECT';
  text.params.position.value = [cx - 88, 1545];
  text.params.size.value = 60;
  chain(title, [text, ...(effect ? [effect] : [])], '#40586b');
  label(title, cx - 125, 1645);
}
for (const [cx, title, close] of [
  [180, 'Open reference', false],
  [540, '17  Set Closure', true],
]) {
  const source = node('Imported Path');
  source.rawPath = `M${cx - 70} 1830L${cx - 70} 1740L${cx + 70} 1740L${cx + 70} 1830`;
  const stroke = node('Stroke');
  stroke.params.width.value = 7;
  stroke.params.color.value = { hex: '#b05e3d', alpha: 1 };
  chain(title, [source, ...(close ? [node('Set Closure')] : []), stroke], '#b05e3d', false);
  label(title, cx - 125, 1905);
}
const twistStar = node('Star');
twistStar.params.center.value = [890, 1790];
twistStar.params.outerRadius.value = 78;
twistStar.params.innerRadius.value = 35;
chain(
  'Radial twist',
  [twistStar, modifier('Resample Path', { count: 96 }), modifier('Twist Points', { angle: 180 })],
  '#4b746d',
);
label('18  Twist Points', 765, 1905);
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
