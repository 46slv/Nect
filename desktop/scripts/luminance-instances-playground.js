import fs from 'node:fs';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import opentype from 'opentype.js';
import { document, layer, node, serialize, deserialize, evaluate, exportSVG } from '../src/core.js';
const root = new URL('../', import.meta.url),
  bytes = fs.readFileSync(new URL('public/fonts/DejaVuSans.ttf', root));
const fonts = {
  default: opentype.parse(
    bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
  ),
};
const doc = document();
doc.name = 'Luminance Instances • size and angle';
doc.width = 960;
doc.height = 360;
doc.layers = [];
function add(name, nodes) {
  const l = layer(name);
  l.id = `luma-instances-${doc.layers.length}`;
  l.nodes = nodes;
  nodes.forEach((n, i) => {
    n.id = `${l.id}:n${i}`;
    n.position = [40 + i * 245, 60];
  });
  l.output = nodes.at(-1).id;
  doc.layers.push(l);
  return l;
}
function chain(name, nodes) {
  const l = add(name, nodes);
  nodes.slice(1).forEach((n, i) => (n.params.path.source = nodes[i].id));
  return l;
}
function label(text, x, y, size = 14) {
  const n = node('Text');
  n.text = text;
  n.params.position.value = [x, y];
  n.params.size.value = size;
  chain(text, [n, node('Fill'), node('Output')]);
}
const bg = node('Rectangle'),
  white = node('Fill');
bg.params.center.value = [480, 180];
bg.params.size.value = [960, 360];
white.params.color.value = { hex: '#ffffff', alpha: 1 };
chain('Paper', [bg, white, node('Output')]);
label('Luminance → motif size + angle', 28, 38, 25);
label('One editable curved motif. White → black controls scale and clockwise rotation.', 28, 64);
for (let panel = 0; panel < 3; panel++) {
  const x = 32 + panel * 312,
    y = 128,
    shape = node('Rectangle'),
    gradient = node('Linear Gradient Fill');
  shape.params.center.value = [x + 128, y + 80];
  shape.params.size.value = [256, 160];
  gradient.params.start.value = [x, y];
  gradient.params.end.value = [x + 256, y];
  gradient.params.startColor.value = { hex: '#ffffff', alpha: 1 };
  gradient.params.endColor.value = { hex: '#000000', alpha: 1 };
  label(['01  SOURCE DARKNESS', '02  SCALE ONLY', '03  SCALE + ANGLE'][panel], x, 104);
  if (!panel) {
    chain('White to black source', [shape, gradient, node('Output')]);
    label('Same sRGB gradient / linear-light samples', x, 315, 11);
    continue;
  }
  const motif = node('Imported Path'),
    effect = node('Luminance Instances'),
    fill = node('Fill'),
    out = node('Output');
  motif.rawPath = 'M-12 -3 C-4 -8 7 -7 12 0 C7 2 -5 7 -12 3Z';
  effect.params.pitch.value = 32;
  effect.params.gridOrigin.value = [x, y];
  effect.params.minScale.value = 0.15;
  effect.params.maxScale.value = 1;
  effect.params.maxRotation.value = panel === 1 ? 0 : 120;
  fill.params.color.value = { hex: panel === 1 ? '#245a77' : '#ad492b', alpha: 1 };
  add(panel === 1 ? 'Darkness maps size' : 'Darkness maps size and angle', [
    shape,
    gradient,
    motif,
    effect,
    fill,
    out,
  ]);
  gradient.params.path.source = shape.id;
  effect.params.path.source = gradient.id;
  effect.params.motif.source = motif.id;
  fill.params.path.source = effect.id;
  out.params.path.source = fill.id;
  label(panel === 1 ? 'Scale 0.15 → 1 / Angle 0°' : 'Scale 0.15 → 1 / Angle 0° → 120°', x, 315, 11);
}
const restored = deserialize(serialize(doc));
for (let i = 0; i < doc.layers.length; i++) {
  const a = evaluate(doc.layers[i], fonts),
    b = evaluate(restored.layers[i], fonts);
  assert.equal(a.error, null);
  assert.equal(b.error, null);
  assert.deepEqual(b.path, a.path);
}
const effects = restored.layers.filter((l) =>
  l.nodes.some((n) => n.type === 'Luminance Instances'),
);
const expected = effects.map((l) =>
  evaluate(l).run(l.nodes.find((n) => n.type === 'Luminance Instances').id),
);
const cold = execFileSync(
  process.execPath,
  [
    '--input-type=module',
    '-e',
    `import fs from 'node:fs';import {deserialize,evaluate} from './src/core.js';const d=deserialize(fs.readFileSync(0,'utf8'));console.log(JSON.stringify(d.layers.filter(l=>l.nodes.some(n=>n.type==='Luminance Instances')).map(l=>evaluate(l).run(l.nodes.find(n=>n.type==='Luminance Instances').id))));`,
  ],
  { input: serialize(restored), encoding: 'utf8' },
);
// JSON canonicalizes signed zero; compare the complete serialized geometry byte-for-byte.
assert.equal(cold.trim(), JSON.stringify(expected));
fs.writeFileSync(new URL('examples/luminance-instances.json', root), serialize(restored) + '\n');
fs.writeFileSync(new URL('artifacts/luminance-instances.svg', root), exportSVG(restored, fonts));
console.log(
  `Verified ${restored.layers.length} editable layers, ${expected.reduce((n, p) => n + p.contours.length, 0)} motifs, exact fresh-process reopen, schema ${restored.schemaVersion}.`,
);
