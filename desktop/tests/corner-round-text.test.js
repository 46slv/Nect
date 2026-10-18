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

for (const raw of ['B', 'S', 'a', 'g']) {
  test(`real ${raw} sharp cubic joins round at useful font scales, preserving topology and reopening`, async () => {
    const { fontScene } = await import('../scripts/cubic-font-corners-example.js');
    for (const serif of [false, true])
      for (const size of [48, 96])
        for (const radius of [0.25, 2]) {
          const s = fontScene(raw, { serif, size, radius }),
            e = evaluate(s.l, resolveFonts(s.doc));
          assert.equal(e.error, null);
          const source = e.run(s.text.id),
            result = e.run(s.round.id);
          assert.ok(result.cornerDiagnostics.rounded > 0);
          if (!serif)
            assert.equal(result.cornerDiagnostics.rounded, { B: 7, S: 7, a: 7, g: 6 }[raw]);
          assert.equal(result.contours.length, source.contours.length);
          assert.notEqual(pathData(result), pathData(source));
          assert.deepEqual(
            result.contours.map((c) => [c.group, c.closed]),
            source.contours.map((c) => [c.group, c.closed]),
          );
          const reopened = deserialize(serialize(s.doc));
          assert.equal(exportSVG(reopened), exportSVG(s.doc));
          const direct = fontScene(raw, { serif, size, radius, converted: true });
          assert.equal(
            pathData(e.path),
            pathData(evaluate(direct.l, resolveFonts(direct.doc)).path),
          );
        }
  });
}
test('real O is exactly unchanged over small and oversized radii and font sizes', async () => {
  const { fontScene } = await import('../scripts/cubic-font-corners-example.js');
  for (const serif of [false, true])
    for (const size of [48, 96])
      for (const radius of [0.25, 2, 24, 1000]) {
        const s = fontScene('O', { serif, size, radius }),
          e = evaluate(s.l, resolveFonts(s.doc));
        assert.equal(e.error, null);
        assert.deepEqual(
          e.run(s.round.id).contours.map(geometry),
          e.run(s.text.id).contours.map(geometry),
        );
      }
});
test('static TTF/glyf and OTF/CFF imports both feed Corner Round and cold SVG export', () => {
  for (const extension of ['ttf', 'otf']) {
    const bytes = fs.readFileSync(
      new URL(`./fixtures/fonts/NectJPFixture-Regular.${extension}`, import.meta.url),
    );
    const font = makeFontAsset(
      bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
      `fixture.${extension}`,
    );
    const s = scene('ABC日本');
    s.doc.fontAssets = { [font.id]: font.asset };
    s.text.font = font.id;
    const e = evaluate(s.l, resolveFonts(s.doc));
    assert.equal(e.error, null);
    assert.equal(exportSVG(deserialize(serialize(s.doc))), exportSVG(s.doc));
    assert.ok(
      e.path.contours.every((c) =>
        c.points.every((p) => [...p.position, ...p.handleA, ...p.handleB].every(Number.isFinite)),
      ),
    );
  }
});
test('real B counters retain their interior samples at oversized radius', async () => {
  const { default: paper } = await import('paper');
  const { fontScene } = await import('../scripts/cubic-font-corners-example.js');
  const project = new paper.Project();
  const curve = (c) =>
    new paper.Path({
      segments: c.points.map((p) => new paper.Segment(p.position, p.handleA, p.handleB)),
      closed: c.closed,
      insert: false,
    });
  try {
    for (const serif of [false, true]) {
      const s = fontScene('B', { serif, size: 48, radius: 24 }),
        e = evaluate(s.l, resolveFonts(s.doc));
      const before = e.run(s.text.id).contours,
        after = e.run(s.round.id).contours;
      assert.equal(before.length, 3);
      for (let i = 0; i < before.length; i++) {
        const a = curve(before[i]),
          b = curve(after[i]);
        assert.equal(Math.sign(a.area), Math.sign(b.area));
        assert.ok(b.contains(a.interiorPoint));
        a.remove();
        b.remove();
      }
    }
  } finally {
    project.remove();
  }
});
