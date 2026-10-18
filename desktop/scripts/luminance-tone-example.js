// Build an editable comparison of the native luminance modifiers and export proof.
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
doc.name = 'Luminance Tone Power Comparison';
doc.width = 816;
doc.height = 790;
doc.layers = [];

function addLayer(name, nodes) {
  const l = layer(name);
  l.id = `luma-tone-${doc.layers.length}`;
  l.nodes = nodes;
  nodes.forEach((n, i) => {
    n.id = `${l.id}:n${i}`;
    n.position = [48 + i * 250, 56];
  });
  l.output = nodes.at(-1).id;
  nodes.slice(1).forEach((n, i) => connect(l, nodes[i].id, n.id, 'path'));
  doc.layers.push(l);
  return l;
}

function label(text, x, y, size = 14, color = '#34434d') {
  const textNode = node('Text'),
    fill = node('Fill'),
    out = node('Output');
  textNode.text = text;
  textNode.params.position.value = [x, y];
  textNode.params.size.value = size;
  fill.params.color.value = { hex: color, alpha: 1 };
  addLayer(`Label / ${text}`, [textNode, fill, out]);
}

// Opaque artboard backing makes the rendered proof visibly white in any viewer.
const background = node('Rectangle'),
  white = node('Fill'),
  backgroundOutput = node('Output');
background.params.center.value = [doc.width / 2, doc.height / 2];
background.params.size.value = [doc.width, doc.height];
white.params.color.value = { hex: '#ffffff', alpha: 1 };
addLayer('White paper', [background, white, backgroundOutput]);

label('Luminance → Tone Power', 32, 40, 25, '#20323d');
label(
  'Same gradient and grid. Power acts on darkness after alpha + Invert.',
  32,
  66,
  14,
  '#697984',
);
const types = [
  'Halftone',
  'Ordered Dither',
  'Line Screen',
  'Error Diffusion Dither',
  'Luminance Instances',
];
for (let column = 0; column < 3; column++) {
  const power = [0.5, 1, 2][column],
    x = 32 + column * 256;
  label(`Power ${power}${power === 1 ? ' · unchanged default' : ''}`, x, 99, 15);
  for (let row = 0; row < types.length; row++) {
    const y = 130 + row * 128,
      type = types[row],
      rectangle = node('Rectangle'),
      gradient = node('Linear Gradient Fill'),
      effect = node(type),
      out = node('Output');
    rectangle.params.center.value = [x + 112, y + 40];
    rectangle.params.size.value = [224, 80];
    gradient.params.start.value = [x, y];
    gradient.params.end.value = [x + 224, y];
    gradient.params.startColor.value = { hex: '#ffffff', alpha: 1 };
    gradient.params.endColor.value = { hex: '#000000', alpha: 1 };
    effect.params.pitch.value = 16;
    effect.params.tonePower.value = power;
    const nodes = [rectangle, gradient, effect];
    if (type === 'Luminance Instances') nodes.push(node('Fill'));
    nodes.push(out);
    const l = addLayer(`${type} / power ${power}`, nodes);
    if (type === 'Luminance Instances') {
      const motif = node('Imported Path');
      motif.id = `${l.id}:motif`;
      motif.rawPath = 'M-7 -2 L7 0 L-7 2Z';
      motif.position = [48, 220];
      l.nodes.push(motif);
      effect.params.motif.source = motif.id;
      effect.params.minScale.value = 0;
      effect.params.maxScale.value = 1;
      nodes[3].params.color.value = { hex: '#000000', alpha: 1 };
    }
    label(type, x, y + 102, 13);
  }
}
const restored = deserialize(serialize(doc)),
  saved = serialize(restored),
  svg = exportSVG(restored, fonts);
const expected = restored.layers.map((l) => {
  const result = evaluate(l, fonts, restored.imageAssets);
  if (result.error) throw Error(`${l.name}: ${result.error}`);
  return result.path;
});
fs.writeFileSync(new URL('examples/luminance-tone-power.json', root), saved + '\n');
fs.writeFileSync(new URL('artifacts/luminance-tone-power.svg', root), svg);
// A separate process reloads only the saved file, with fresh sampler/evaluator caches.
const { execFileSync } = await import('node:child_process');
const actual = execFileSync(
  process.execPath,
  [
    '--input-type=module',
    '-e',
    `
import fs from 'node:fs'; import opentype from 'opentype.js';
import {deserialize,evaluate,exportSVG} from './src/core.js';
const bytes=fs.readFileSync('public/fonts/DejaVuSans.ttf');
const fonts={default:opentype.parse(bytes.buffer.slice(bytes.byteOffset,bytes.byteOffset+bytes.byteLength))};
const doc=deserialize(fs.readFileSync('examples/luminance-tone-power.json','utf8'));
const result=doc.layers.map(l=>{const r=evaluate(l,fonts,doc.imageAssets);if(r.error)throw Error(r.error);return r.path;});
if(exportSVG(doc,fonts)!==fs.readFileSync('artifacts/luminance-tone-power.svg','utf8'))throw Error('Cold SVG mismatch');
process.stdout.write(JSON.stringify(result));
`,
  ],
  { cwd: root, maxBuffer: 8 * 1024 * 1024 },
).toString();
if (actual !== JSON.stringify(expected))
  throw Error('Cold reopen changed geometry or fingerprints');
console.log(
  `Verified ${restored.layers.length} editable layers, 15 tone comparisons; fresh-process geometry, fingerprints and SVG are identical.`,
);
