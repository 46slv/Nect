// Build an editable comparison of the native luminance modifiers and export proof.
import fs from 'node:fs';
import opentype from 'opentype.js';
import { deflateSync } from 'node:zlib';
import { validateImageAssets } from '../src/images.js';
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
doc.name = 'Image Luminance Playground';
doc.width = 1040;
doc.height = 312;
doc.layers = [];

// Own synthetic RGBA PNG fixture: no external image or decoder is needed to rebuild it.
const pixelWidth = 96,
  pixelHeight = 64;
const raw = Buffer.alloc(pixelHeight * (pixelWidth * 4 + 1));
for (let y = 0; y < pixelHeight; y++) {
  for (let x = 0; x < pixelWidth; x++) {
    const tone = Math.max(
      0,
      Math.min(
        1,
        0.12 +
          (0.76 * x) / (pixelWidth - 1) +
          0.18 * Math.sin((y / pixelHeight) * Math.PI * 2) * Math.sin((x / pixelWidth) * Math.PI),
      ),
    );
    const i = y * (pixelWidth * 4 + 1) + 1 + x * 4;
    raw[i] = raw[i + 1] = raw[i + 2] = Math.round(tone * 255);
    raw[i + 3] = Math.round(255 * (0.7 + (0.3 * y) / (pixelHeight - 1)));
  }
}
function pngChunk(type, data) {
  const name = Buffer.from(type),
    body = Buffer.concat([name, data]);
  let crc = 0xffffffff;
  for (const byte of body) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ (crc & 1 ? 0xedb88320 : 0);
  }
  const length = Buffer.alloc(4),
    checksum = Buffer.alloc(4);
  length.writeUInt32BE(data.length);
  checksum.writeUInt32BE((crc ^ 0xffffffff) >>> 0);
  return Buffer.concat([length, body, checksum]);
}
const ihdr = Buffer.alloc(13);
ihdr.writeUInt32BE(pixelWidth, 0);
ihdr.writeUInt32BE(pixelHeight, 4);
ihdr[8] = 8;
ihdr[9] = 6;
const png = Buffer.concat([
  Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]),
  pngChunk('IHDR', ihdr),
  pngChunk('IDAT', deflateSync(raw)),
  pngChunk('IEND', Buffer.alloc(0)),
]);
doc.imageAssets = {
  'image-tone-raster': {
    name: 'procedural-tone.png',
    mime: 'image/png',
    width: pixelWidth,
    height: pixelHeight,
    data: png.toString('base64'),
  },
};
validateImageAssets(doc.imageAssets);

function addLayer(name, nodes) {
  const l = layer(name);
  l.id = `image-luma-${doc.layers.length}`;
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

label('PNG → Luminance Geometry', 32, 40, 25, '#20323d');
label(
  'Embedded raster pixels → dots, ordered dither and variable-width lines',
  32,
  66,
  14,
  '#697984',
);

const panelX = [32, 288, 544, 800],
  panelY = 120,
  panelWidth = 192,
  panelHeight = 128,
  pitch = 8,
  columns = [
    ['01  SOURCE PNG', '96×64 embedded RGBA raster'],
    ['02  HALFTONE', 'pixel luminance → dot area'],
    ['03  ORDERED DITHER', 'pixel luminance → Bayer cells'],
    ['04  LINE SCREEN', 'pixel luminance → line width'],
  ];

for (let index = 0; index < panelX.length; index++) {
  const x = panelX[index];
  const source = node('Image');
  source.asset = 'image-tone-raster';
  source.params.position.value = [x, panelY];
  source.params.size.value = [panelWidth, panelHeight];
  const nodes = [source];
  if (index > 0) {
    const modifier = node(['', 'Halftone', 'Ordered Dither', 'Line Screen'][index]);
    modifier.params.pitch.value = pitch;
    if (index === 3) modifier.operation = 'horizontal';
    nodes.push(modifier);
  }
  nodes.push(node(index === 0 ? 'Image Output' : 'Output'));
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
fs.writeFileSync(new URL('examples/image-luminance-playground.json', root), saved + '\n');
fs.writeFileSync(new URL('artifacts/image-luminance-playground.svg', root), svg);
console.log(`Verified ${restored.layers.length} editable layers and identical reopened outputs.`);
