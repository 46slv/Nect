import fs from 'node:fs';
import { pathToFileURL } from 'node:url';
import { createHash } from 'node:crypto';
import {
  document,
  layer,
  node,
  connect,
  evaluate,
  serialize,
  deserialize,
  exportSVG,
  pathData,
} from '../src/core.js';
import { makeFontAsset, resolveFonts } from '../src/fonts.js';
const root = new URL('../', import.meta.url);
const cache = new Map();
export function fontAsset(serif = false) {
  if (cache.has(serif)) return cache.get(serif);
  const filename = serif ? 'tests/fixtures/DejaVuSerif.ttf' : 'public/fonts/DejaVuSans.ttf',
    bytes = fs.readFileSync(new URL(filename, root));
  const asset = makeFontAsset(
    bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
    serif ? 'DejaVuSerif.ttf' : 'DejaVuSans.ttf',
  );
  cache.set(serif, asset);
  return asset;
}
export function fontScene(raw, { serif = false, size = 48, radius = 2, converted = false } = {}) {
  const doc = document(),
    l = doc.layers[0],
    text = node('Text'),
    round = node('Corner Round'),
    fill = node('Fill'),
    out = node('Output'),
    font = fontAsset(serif);
  doc.fontAssets = { [font.id]: font.asset };
  text.font = font.id;
  text.text = raw;
  text.params.size.value = size;
  round.params.radius.value = radius;
  l.nodes = [text, ...(converted ? [node('Text to Path')] : []), round, fill, out];
  l.output = out.id;
  l.nodes.forEach((n, i) => {
    if (i) connect(l, l.nodes[i - 1].id, n.id, 'path');
  });
  return { doc, l, text, round };
}
export function generate() {
  const doc = document();
  doc.name = 'Cubic font corner evidence';
  doc.width = 1800;
  doc.height = 1650;
  doc.layers = [];
  doc.fontAssets = {};
  const evidence = [];
  let row = 0;
  for (const serif of [false, true])
    for (const size of [48, 96])
      for (const radius of [0.25, 2, 24]) {
        for (const [column, r] of [
          [0, 0],
          [1, radius],
        ]) {
          const s = fontScene('B S a g O HO HI', { serif, size, radius: r });
          Object.assign(doc.fontAssets, s.doc.fontAssets);
          s.l.name = `${serif ? 'Serif' : 'Sans'} ${size}px ${r}px`;
          s.text.params.position.value = [40 + column * 880, 80 + row * 130];
          doc.layers.push(s.l);
          const e = evaluate(s.l, resolveFonts(s.doc));
          if (e.error) throw Error(e.error);
          if (column)
            evidence.push({
              font: serif ? 'DejaVu Serif' : 'DejaVu Sans',
              size,
              radius,
              diagnostics: e.run(s.round.id).cornerDiagnostics,
            });
        }
        const label = fontScene(
          `${serif ? 'DejaVu Serif' : 'DejaVu Sans'} | ${size} px | source left, rounded right | nominal radius ${radius} px`,
          { size: 12, radius: 0 },
        );
        Object.assign(doc.fontAssets, label.doc.fontAssets);
        label.text.params.position.value = [40, 108 + row * 130];
        doc.layers.push(label.l);
        row++;
      }
  const reopened = deserialize(serialize(doc));
  if (exportSVG(reopened) !== exportSVG(doc)) throw Error('cold reopen SVG differs');
  fs.writeFileSync(new URL('examples/cubic-font-corners.json', root), serialize(doc));
  fs.writeFileSync(new URL('artifacts/cubic-font-corners.svg', root), exportSVG(doc));
  const matrix = [];
  for (const raw of ['B', 'S', 'a', 'g', 'O', 'HO', 'HI'])
    for (const serif of [false, true])
      for (const size of [48, 96])
        for (const radius of [0.25, 2, 24]) {
          const s = fontScene(raw, { serif, size, radius }),
            e = evaluate(s.l, resolveFonts(s.doc));
          if (e.error) throw Error(e.error);
          const t = fontScene(raw, { serif, size, radius, converted: true }),
            f = evaluate(t.l, resolveFonts(t.doc));
          if (pathData(e.path) !== pathData(f.path)) throw Error('direct and Text to Path differ');
          matrix.push({
            text: raw,
            font: serif ? 'serif' : 'sans',
            size,
            radius,
            ...e.run(s.round.id).cornerDiagnostics,
          });
        }
  fs.writeFileSync(
    new URL('artifacts/cubic-font-corners-verification.json', root),
    JSON.stringify(
      {
        coldReopenSVG: true,
        directMatchesTextToPath: true,
        serifSHA256: createHash('sha256')
          .update(fs.readFileSync(new URL('tests/fixtures/DejaVuSerif.ttf', root)))
          .digest('hex'),
        columns: ['source radius 0', 'rounded'],
        rows: evidence,
        matrix,
      },
      null,
      2,
    ) + '\n',
  );
  console.log(JSON.stringify(evidence, null, 2));
}
if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) generate();
