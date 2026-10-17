import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { createHash } from 'node:crypto';
import {
  document,
  node,
  connect,
  evaluate,
  serialize,
  deserialize,
  exportSVG,
  pathData,
} from '../src/core.js';
import { makeFontAsset, resolveFonts } from '../src/fonts.js';
import { roundCorners } from '../src/corner-round-geometry.js';
const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
const imported = makeFontAsset(
  bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
  'DejaVuSans.ttf',
);
const geometry = (c) =>
  c.points.map(({ position, handleA, handleB }) => ({ position, handleA, handleB }));
function scene(value) {
  const doc = document(),
    l = doc.layers[0],
    text = node('Text'),
    round = node('Corner Round'),
    out = node('Output');
  doc.fontAssets = { [imported.id]: imported.asset };
  text.font = imported.id;
  text.text = value;
  round.params.radius.value = 2;
  l.nodes = [text, round, out];
  l.output = out.id;
  connect(l, text.id, round.id, 'path');
  connect(l, round.id, out.id, 'path');
  return { doc, l, text, round };
}
for (const [raw, before, after] of [
  ['H', 12, 24],
  ['O', 24, 24],
  ['HO', 36, 48],
]) {
  test(`real DejaVu Sans ${raw}: direct Text rounding preserves curves, source edits, and reopened export`, () => {
    const { doc, l, text, round } = scene(raw);
    const original = structuredClone(text);
    const e = evaluate(l, resolveFonts(doc));
    assert.equal(e.error, null);
    const source = e.run(text.id),
      result = e.run(round.id);
    const count = (path) => path.contours.reduce((n, c) => n + c.points.length, 0);
    assert.equal(count(source), before);
    assert.equal(count(result), after);
    for (let i = 0; i < source.contours.length; i++) {
      if (
        source.contours[i].points.some((p) => [...p.handleA, ...p.handleB].some((v) => v !== 0))
      ) {
        assert.deepEqual(geometry(result.contours[i]), geometry(source.contours[i]));
      }
      assert.equal(result.contours[i].group, source.contours[i].group);
    }
    assert.deepEqual(text, original);
    const reopened = deserialize(serialize(doc));
    assert.equal(
      pathData(evaluate(reopened.layers[0], resolveFonts(reopened)).path),
      pathData(e.path),
    );
    assert.equal(exportSVG(reopened), exportSVG(doc));
    text.text = 'HO';
    assert.equal(evaluate(l, resolveFonts(doc)).error, null);
    round.params.radius.value = 0;
    assert.deepEqual(
      evaluate(l, resolveFonts(doc)).run(round.id).contours,
      evaluate(l, resolveFonts(doc)).run(text.id).contours,
    );
  });
}
test('straight-only geometry fingerprints remain unchanged', () => {
  const fixtures = [
    [
      true,
      [
        [0, 0],
        [10, 0],
        [10, 10],
        [0, 10],
      ],
      'fb3f865e054527cb843d197824722a1c3c7a14bfd61891168ea30d5702388e49',
    ],
    [
      false,
      [
        [0, 0],
        [5, 0],
        [10, 0],
        [10, 10],
      ],
      'da7225117b0ff23fbc33975c585eb409eb19af173ee546d632e05db057b95e34',
    ],
  ];
  for (const [closed, positions, expected] of fixtures) {
    const c = {
      closed,
      points: positions.map((position, i) => ({
        id: `p${i}`,
        position,
        handleA: [0, 0],
        handleB: [0, 0],
      })),
    };
    const output = roundCorners({ contours: [c] }, { radius: 2, prefix: 'r' });
    assert.equal(createHash('sha256').update(JSON.stringify(output)).digest('hex'), expected);
  }
});
