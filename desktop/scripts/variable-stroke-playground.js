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
doc.name = 'Variable Stroke Playground';
doc.width = 816;
doc.height = 312;
doc.layers = [];

function addLayer(name, nodes) {
  const l = layer(name);
  l.id = `variable-stroke-${doc.layers.length}`;
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

label('Stroke → Variable Width', 32, 40, 25, '#20323d');
label('Same curve and base width; change the start and end multipliers', 32, 66, 14, '#697984');

const panelX = [32, 312, 592],
  panelY = 120,
  panelWidth = 192,
  panelHeight = 128,
  pitch = 8,
  columns = [
    ['01  UNIFORM', 'start 1× / end 1×'],
    ['02  START TIP', 'start 0× / end 1×'],
    ['03  END TIP', 'start 1× / end 0×'],
  ];

for (let index = 0; index < panelX.length; index++) {
  const x = panelX[index],
    y = panelY + panelHeight / 2;
  const curve = node('Imported Path');
  curve.rawPath = `M${x} ${y} C${x + 60} ${y - 90} ${x + 132} ${y + 90} ${x + 192} ${y}`;
  const stroke = node('Stroke');
  stroke.params.width.value = 24;
  stroke.strokeCap = 'round';
  stroke.params.color.value = { hex: '#20323d', alpha: 1 };
  const expand = node('Expand Stroke');
  if (!expand.params.startWidthMultiplier) throw Error('Variable-width integration is not ready');
  expand.params.startWidthMultiplier.value = index === 1 ? 0 : 1;
  expand.params.endWidthMultiplier.value = index === 2 ? 0 : 1;
  const fill = node('Fill');
  fill.params.color.value = { hex: '#20323d', alpha: 1 };
  const nodes = [curve, stroke, expand, fill, node('Output')];
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
fs.writeFileSync(new URL('examples/variable-stroke-playground.json', root), saved + '\n');
fs.writeFileSync(new URL('artifacts/variable-stroke-playground.svg', root), svg);
console.log(`Verified ${restored.layers.length} editable layers and identical reopened outputs.`);
