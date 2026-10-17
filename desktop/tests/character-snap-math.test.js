import test from 'node:test';
import assert from 'node:assert/strict';
import { guideDefaults, snapTranslation } from '../src/guides.js';
import { inverseCharacterDelta, transformCharacterPoint } from '../src/character-geometry.js';

const doc = () => ({
  width: 1000,
  height: 1000,
  editor: {
    ...guideDefaults(),
    snap: true,
    snapGuides: false,
    snapArtboard: false,
    snapGrid: false,
    snapArtwork: false,
  },
});

test('current solver has no implicit sibling-character target', () => {
  const result = snapTranslation(
    doc(),
    [
      [20, 25, 30],
      [60, 70, 80],
    ],
    [0, 17],
    1,
  );
  // A sibling ink bottom of 100 is three pixels away, but is never supplied.
  assert.deepEqual(result, { delta: [0, 17], matches: [] });
});

test('existing supplied-target tolerance is six CSS pixels across zoom levels', () => {
  for (const zoom of [0.5, 1, 2]) {
    const d = doc();
    d.editor.snapGuides = true;
    d.editor.guides = [{ id: 'g1', axis: 'y', position: 100, locked: false }];
    const inside = snapTranslation(d, [[], [80]], [0, 20 - 5.9 / zoom], zoom);
    assert.ok(Math.abs(inside.delta[1] - 20) < 1e-10);
    assert.equal(inside.matches.length, 1);
    const outside = snapTranslation(d, [[], [80]], [0, 20 - 6.1 / zoom], zoom);
    assert.equal(outside.matches.length, 0);
    const bypass = snapTranslation(d, [[], [80]], [0, 19], zoom, true);
    assert.deepEqual(bypass, { delta: [0, 19], matches: [] });
    const forbidden = snapTranslation(d, [[], []], [7, 0], zoom);
    assert.deepEqual(forbidden, { delta: [7, 0], matches: [] });
  }
});

test('snapped artboard translation round-trips through rotated reflected affine', () => {
  const m = [0, 2, 3, 0, 150, -80];
  const local = inverseCharacterDelta(m, [12, -8]);
  const start = transformCharacterPoint(m, [17, 29]);
  const end = transformCharacterPoint(m, [17 + local[0], 29 + local[1]]);
  assert.deepEqual(
    end.map((v, i) => v - start[i]),
    [12, -8],
  );
  assert.throws(() => inverseCharacterDelta([0, 0, 0, 0, 0, 0], [1, 1]), /singular/);
});

import {
  characterInkBounds,
  collectCharacterSnapGeometry,
  characterGapGuidePath,
} from '../src/character-snapping.js';
const occurrence = { matrix: [1, 0, 0, 1, 0, 0], route: 'text' };
const glyph = (id, x, y, w, h, sourceIndex = Number(id.slice(1)) - 1) => ({
  id,
  sourceIndex,
  path: `M${x},${y}h${w}v${h}h${-w}z`,
  whitespace: false,
});
const geometry = (characters, selectedIds, extra = {}) =>
  collectCharacterSnapGeometry({
    characters,
    selectedIds,
    occurrence,
    lines: [{ sourceStart: 0, sourceEnd: 100 }],
    ...extra,
  });
const solve = (g, delta, scale = 1, bypass = false, d = doc()) =>
  snapTranslation(d, g.sources, delta, scale, bypass, null, [], [], g.candidates);
const near = (a, b) => assert.ok(Math.abs(a - b) < 1e-7, `${a} != ${b}`);

test('transformed cubic ink extrema differ from transformed local AABB', () => {
  const path = 'M0,0C0,100 100,100 100,0Z',
    m = [1, 0, 1, 1, 0, 0];
  const actual = characterInkBounds(path, m);
  near(actual[2], 100 * Math.SQRT2); // x(t)=300t-200t^3, extremum t=1/sqrt(2).
  near(actual[3], 75);
  assert.notEqual(actual[2], 175, 'transformed local AABB is overlarge');
  assert.equal(characterInkBounds(path, [0, 0, 0, 0, 0, 0]), null);
  assert.equal(characterInkBounds('', m), null);
});

test('sibling edges and centers share one solver, independent of artwork flag', () => {
  const g = geometry([glyph('c1', 0, 0, 20, 30), glyph('c2', 100, 100, 40, 60)], ['c1']);
  for (const [raw, expected, position, source] of [
    [127, 130, 160, 30],
    [112, 115, 130, 15],
    [97, 100, 100, 0],
  ]) {
    const r = solve({ ...g, sources: [[], [source]] }, [0, raw]);
    near(r.delta[1], expected);
    assert.equal(r.matches.find((m) => m.axis === 1).position, position);
  }
  for (const zoom of [0.5, 1, 2]) {
    const only = {
      ...g,
      sources: [[], [30]],
      candidates: g.candidates.filter((c) => c.axis === 1 && c.position === 160),
    };
    near(solve(only, [0, 130 - 5.9 / zoom], zoom).delta[1], 130);
    assert.equal(solve(only, [0, 130 - 6.1 / zoom], zoom).matches.length, 0);
  }
  assert.deepEqual(solve(g, [7, 127], 1, true), { delta: [7, 127], matches: [] });
  const off = doc();
  off.editor.snap = false;
  assert.deepEqual(solve(g, [7, 127], 1, false, off), { delta: [7, 127], matches: [] });
  const guide = doc();
  guide.editor.snapGuides = true;
  guide.editor.guides = [{ id: 'g', axis: 'y', position: 160 }];
  assert.equal(solve(g, [0, 127], 1, false, guide).matches.find((m) => m.axis === 1).kind, 'guide');
  assert.deepEqual(solve({ ...g, sources: [[], []] }, [0, 127]).matches, []);
});

test('equal-gap between neighbors, either end, vertical and rigid block; paired marks', () => {
  const rows = [glyph('c1', 0, 0, 10, 20), glyph('c2', 25, 0, 20, 30), glyph('c3', 70, 0, 30, 40)];
  const middle = geometry(rows, ['c2']),
    r = solve(middle, [3, 0]);
  near(r.delta[0], 5);
  const match = r.matches.find((m) => m.kind === 'character-gap');
  assert.deepEqual(match.segments, [
    [10, 30],
    [50, 70],
  ]);
  assert.equal(match.gap, 20);
  assert.equal((characterGapGuidePath(match).match(/M/g) || []).length, 6);
  near(solve(geometry(rows, ['c3']), [-8, 0]).delta[0], -10);
  near(solve(geometry(rows, ['c1']), [-8, 0]).delta[0], -10);
  const vertical = geometry(
    [glyph('c1', 0, 0, 20, 10), glyph('c2', 0, 25, 30, 20), glyph('c3', 0, 70, 40, 30)],
    ['c2'],
    { writingMode: 'vertical-rl' },
  );
  near(solve(vertical, [0, 3]).delta[1], 5);
  const block = geometry(
    [
      glyph('c1', 0, 0, 10, 20),
      glyph('c2', 25, 0, 10, 20),
      glyph('c3', 40, 0, 5, 20),
      glyph('c4', 70, 0, 10, 20),
    ],
    ['c2', 'c3'],
  );
  near(solve(block, [3, 0]).delta[0], 5);
  assert.ok(block.candidates.every((c) => !['c2', 'c3'].includes(c.targetId)));
});

test('whitespace, other lines, interleaving, unsupported mapping never create spacing targets', () => {
  const rows = [glyph('c1', 0, 0, 10, 20), glyph('c2', 25, 0, 20, 30), glyph('c3', 70, 0, 30, 40)];
  const count = (g) => g.candidates.filter((c) => c.kind === 'character-gap').length;
  assert.equal(count(geometry(rows, ['c1', 'c3'])), 0);
  assert.equal(
    count(
      geometry(rows, ['c2'], {
        lines: [
          { sourceStart: 0, sourceEnd: 2 },
          { sourceStart: 2, sourceEnd: 3 },
        ],
      }),
    ),
    0,
  );
  assert.equal(count(geometry([rows[0], glyph('c2', 5, 0, 20, 30), rows[2]], ['c2'])), 0);
  assert.equal(count(geometry([rows[0], { ...rows[1], whitespace: true }, rows[2]], ['c2'])), 0);
  const spaces = geometry([rows[0], { ...rows[1], whitespace: true }, rows[2]], ['c1']);
  assert.ok(spaces.candidates.every((c) => c.targetId !== 'c2'));
  assert.deepEqual(
    geometry(rows, ['c2'], { occurrence: { ...occurrence, reason: 'unknown' } }).sources,
    [[], []],
  );
});

import fs from 'node:fs';
import { parseFont } from '../src/fonts.js';
import { layoutText } from '../src/text-layout.js';
import {
  setCharacterStyles,
  setCharacterOffsets,
  setCharacterTransforms,
} from '../src/character-identity.js';
const fontBytes = (name) => {
  const b = fs.readFileSync(new URL(name, import.meta.url));
  return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
};
const latin = parseFont(fontBytes('../public/fonts/DejaVuSans.ttf'));
const jp = parseFont(fontBytes('./fixtures/fonts/NectJPFixture-Regular.otf'));

test('actual mixed-font ink and vertical outlines: final offsets reproduce snapped transformed bounds', (t) => {
  for (const writingMode of ['horizontal-tb', 'vertical-rl']) {
    const text = writingMode === 'vertical-rl' ? 'A、' : 'Ag';
    let record = setCharacterStyles(text, undefined, {
      c1: { font: 'default', size: 40 },
      c2: { font: 'font-jp', size: 70 },
    });
    record = setCharacterOffsets(text, record, { c2: [11, 40] });
    record = setCharacterTransforms(text, record, { c2: { rotation: 17, scale: 1.2 } });
    const options = {
      position: [100, 200],
      size: 40,
      fonts: { default: latin, 'font-jp': jp },
      writingMode,
    };
    const layout = layoutText(latin, text, { ...options, characterPositions: record });
    const m = [1.2, 0.5, -0.4, -1.8, 80, 30],
      o = { matrix: m, route: 'affine' };
    const g = geometry(layout.characters, ['c2'], {
      occurrence: o,
      lines: layout.lines,
      writingMode,
    });
    const moving = g.records.find((r) => r.id === 'c2'),
      target = g.records.find((r) => r.id === 'c1');
    const desired = target.bounds[3] - moving.bounds[3];
    const narrow = {
      ...g,
      sources: [[], [moving.bounds[3]]],
      candidates: g.candidates.filter((c) => c.axis === 1 && c.anchor === 2),
    };
    const result = solve(narrow, [0, desired + 2]);
    near(result.delta[1], desired);
    const local = inverseCharacterDelta(m, result.delta),
      offset = [11 + local[0], 40 + local[1]];
    const next = setCharacterOffsets(text, record, { c2: offset });
    const final = layoutText(latin, text, { ...options, characterPositions: next });
    const bounds = characterInkBounds(final.characters[1].path, m);
    assert.ok(Math.abs(bounds[3] - target.bounds[3]) < 0.00005);
    assert.deepEqual(next.styles, record.styles);
    assert.deepEqual(next.transforms, record.transforms);
    assert.equal(final.characters[1].tracking, layout.characters[1].tracking);
    assert.equal(final.characters[1].size, 70);
    assert.equal(result.matches[0].kind, 'character');
    t.diagnostic(
      JSON.stringify({
        writingMode,
        rawY: desired + 2,
        snappedY: desired,
        finalOffset: offset,
        targetBottom: target.bounds[3],
        finalBottom: bounds[3],
        guide: result.matches[0].position,
      }),
    );
  }
});

test('fresh typography geometry changes the next drag snapshot; exact ties are input-order independent', () => {
  const text = 'AgA',
    make = (size) =>
      layoutText(latin, text, {
        position: [0, 100],
        size: 40,
        characterPositions: setCharacterStyles(text, undefined, { c2: { size } }),
      });
  const a = make(40),
    b = make(80),
    ga = geometry(a.characters, ['c2'], { lines: a.lines }),
    gb = geometry(b.characters, ['c2'], { lines: b.lines });
  assert.notDeepEqual(ga.sources, gb.sources);
  const rows = [
    glyph('c1', 100, 0, 10, 20),
    glyph('c2', 0, 0, 10, 20),
    glyph('c3', 100, 0, 10, 20),
  ];
  const first = solve(geometry(rows, ['c2']), [98, 0]);
  const reverse = solve(geometry([...rows].reverse(), ['c2']), [98, 0]);
  assert.deepEqual(first, reverse);
  assert.equal(first.matches[0].targetId, 'c1');
});
