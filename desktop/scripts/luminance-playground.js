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
doc.name = 'Luminance Playground';
doc.width = 816;
doc.height = 312;
doc.layers = [];

function addLayer(name, nodes) {
  const l = layer(name);
  l.id = `luminance-${doc.layers.length}`;
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

label('Luminance → Marks', 32, 40, 25, '#20323d');
label('One grayscale source, three editable outputs', 32, 66, 14, '#697984');

const panelX = [32, 312, 592],
  panelY = 120,
  panelWidth = 192,
  panelHeight = 128,
  pitch = 8,
  columns = [
    ['01  SOURCE GRADIENT', 'linear gradient'],
    ['02  HALFTONE', 'area-calibrated circles'],
    ['03  ORDERED DITHER', 'Bayer 8×8 / 8 px cells'],
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
    const modifier = node(index === 1 ? 'Halftone' : 'Ordered Dither');
    modifier.params.pitch.value = pitch;
    modifier.params.invert.value = false;
    nodes.push(modifier);
  }
  nodes.push(node('Output'));
  addLayer(columns[index][0], nodes);
  label(columns[index][0], x, 106, 14, '#263740');
  label(columns[index][1], x, 274, 12, '#697984');
}

const saved = serialize(doc),
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
fs.writeFileSync(new URL('examples/luminance-playground.json', root), saved + '\n');
fs.writeFileSync(new URL('artifacts/luminance-playground.svg', root), svg);
console.log(`Verified ${restored.layers.length} editable layers and identical reopened outputs.`);
