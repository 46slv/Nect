// Build an editable comparison of the native luminance modifiers and export proof.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
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
doc.name = 'Error Diffusion Comparison';
doc.width = 816;
doc.height = 312;
doc.layers = [];

function addLayer(name, nodes) {
  const l = layer(name);
  l.id = `error-diffusion-${doc.layers.length}`;
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

label('Ordered vs Error Diffusion', 32, 40, 25, '#20323d');
label('Same grayscale field and pitch; two distinct dithering algorithms', 32, 66, 14, '#697984');

const panelX = [32, 312, 592],
  panelY = 120,
  panelWidth = 192,
  panelHeight = 128,
  pitch = 8,
  columns = [
    ['01  SOURCE GRADIENT', 'linear gradient'],
    ['02  ORDERED DITHER', 'fixed Bayer 8×8 thresholds'],
    ['03  ERROR DIFFUSION', 'Floyd–Steinberg, forward raster scan'],
  ];

for (let index = 0; index < panelX.length; index++) {
  const x = panelX[index],
    centerY = panelY + panelHeight / 2,
    rectangle = node('Rectangle'),
    gradient = node('Linear Gradient Fill'),
    nodes = [rectangle, gradient];
  rectangle.params.center.value = [x + panelWidth / 2, centerY];
  rectangle.params.size.value = [panelWidth, panelHeight];
  gradient.params.start.value = [x, centerY];
  gradient.params.end.value = [x + panelWidth, centerY];
  gradient.params.startColor.value = { hex: '#080808', alpha: 1 };
  gradient.params.endColor.value = { hex: '#f8f8f8', alpha: 1 };

  if (index === 1 || index === 2) {
    const modifier = node(index === 1 ? 'Ordered Dither' : 'Error Diffusion Dither');
    modifier.params.gridOrigin.value = [x, panelY];
    modifier.params.pitch.value = pitch;
    modifier.params.invert.value = false;
    nodes.push(modifier);
  }
  nodes.push(node('Output'));
  addLayer(columns[index][0], nodes);
  label(columns[index][0], x, 106, 14, '#263740');
  label(columns[index][1], x, 274, 12, '#697984');
}

// Promote the document before writing the editable schema-19 artifact.
const saved = serialize(deserialize(serialize(doc))),
  restored = deserialize(saved);
if (restored.layers.length !== doc.layers.length) throw Error('Serialized example lost layers');
for (let index = 0; index < doc.layers.length; index++) {
  const originalLayer = doc.layers[index],
    restoredLayer = restored.layers[index],
    before = evaluate(originalLayer, fonts, doc.imageAssets),
    after = evaluate(restoredLayer, fonts, restored.imageAssets);
  if (before.error) throw Error(`${originalLayer.name}: ${before.error}`);
  if (after.error) throw Error(`${restoredLayer.name} after reopen: ${after.error}`);
  if (JSON.stringify(before.path) !== JSON.stringify(after.path))
    throw Error(`${originalLayer.name}: serialized reopen changed evaluated output`);
}

const svg = exportSVG(restored, fonts);
fs.writeFileSync(new URL('examples/error-diffusion-playground.json', root), saved + '\n');
fs.writeFileSync(new URL('artifacts/error-diffusion-playground.svg', root), svg);
const coldCode = `
import fs from 'node:fs';
import opentype from 'opentype.js';
import { deserialize, evaluate, exportSVG } from './src/core.js';
const bytes=fs.readFileSync('public/fonts/DejaVuSans.ttf');
const fonts={default:opentype.parse(bytes.buffer.slice(bytes.byteOffset,bytes.byteOffset+bytes.byteLength))};
const doc=deserialize(fs.readFileSync('examples/error-diffusion-playground.json','utf8'));
process.stdout.write(JSON.stringify({paths:doc.layers.map(l=>evaluate(l,fonts,doc.imageAssets).path),svg:exportSVG(doc,fonts)}));
`;
const cold = JSON.parse(
  execFileSync(process.execPath, ['--input-type=module', '-e', coldCode], {
    cwd: root,
    encoding: 'utf8',
    maxBuffer: 16 * 1024 * 1024,
  }),
);
assert.deepEqual(
  cold.paths,
  restored.layers.map((l) => evaluate(l, fonts, restored.imageAssets).path),
);
assert.equal(cold.svg, svg);
console.log(
  `Verified ${restored.layers.length} editable layers, schema ${restored.schemaVersion}, exact fresh-process evaluated paths and SVG.`,
);
