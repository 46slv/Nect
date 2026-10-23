import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { parseFont } from '../src/fonts.js';
import { layoutText } from '../src/text-layout.js';
import {
  JAPANESE_WRAP_POLICY as policy,
  basicTextWrap,
  composeTextLines,
  validateTextWrap,
  wrapBoundaryAllowed,
  WRAP_OPEN,
  WRAP_NONSTART,
  WRAP_INSEPARABLE,
} from '../src/text-wrap.js';
import { validateDesktopLineSpans } from '../src/desktop-font-text.js';
import {
  document,
  node,
  CommandSession,
  serialize,
  deserialize,
  exportSVG,
  evaluate,
  validate,
  setCharacterStyles,
} from '../src/core.js';
const bytes = fs.readFileSync(
  new URL('./fixtures/fonts/NectJPFixture-Regular.otf', import.meta.url),
);
const font = parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength));
const base = { position: [300, 200], size: 40 };
const layout = (text, options = {}) => layoutText(font, text, { ...base, ...options });
const near = (a, b) => assert.ok(Math.abs(a - b) < 1e-7, `${a} != ${b}`);
function scene(raw) {
  const doc = document(),
    l = doc.layers[0],
    n = node('Text'),
    fill = node('Fill');
  n.text = raw?.text ?? '日本「語、」ぁー。ABC12日本';
  n.params.size.value = raw?.size ?? 40;
  n.params.position.value = base.position;
  n.params.letterSpacing.value = raw?.tracking ?? 0;
  n.params.lineSpacing.value = raw?.lineSpacing ?? 1.2;
  n.alignment = ['start', 'center', 'end'][raw?.alignment ?? 0];
  fill.params.path.source = n.id;
  const out = l.nodes.find((n) => n.id === l.output);
  out.params.path.source = fill.id;
  l.nodes = [n, fill, out];
  const session = new CommandSession(doc, { fonts: () => ({ default: font }) }),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: l.id, nodeId: n.id, ...payload },
      });
  return { n, l, session, send, live: () => session.snapshot().layers[0].nodes[0] };
}
test('explicit basic policy covers named boundaries and never emergency-splits an overlong group', () => {
  validateTextWrap({ width: 80, policy });
  validateTextWrap({ height: 80, policy }, 'vertical-rl');
  for (const bad of [
    { height: 80 },
    { width: 80, policy: undefined },
    { width: 80, policy: 'future' },
    { width: 80, height: 80, policy },
  ])
    assert.throws(() => validateTextWrap(bad));
  assert.throws(() => validateTextWrap({ width: 80, policy }, 'vertical-rl'), /MODE/);
  assert.throws(() => validateTextWrap({ height: 80, policy }), /MODE/);
  for (const char of WRAP_OPEN + WRAP_NONSTART + WRAP_INSEPARABLE) {
    const chars = [...`日${char}${char}本語`];
    for (const budget of [0.1, 1, 2, 4]) {
      const lines = composeTextLines(chars, budget, (a, b) => b - a, policy);
      assert.equal(
        lines.map((l) => chars.slice(l.sourceStart, l.sourceEnd).join('')).join(''),
        chars.join(''),
      );
      for (const l of lines.filter((l) => l.breakKind === 'soft'))
        assert.equal(
          wrapBoundaryAllowed(chars[l.sourceEnd - 1], chars[l.sourceEnd], policy),
          true,
          `U+${char.codePointAt(0).toString(16)}`,
        );
    }
  }
  for (const char of WRAP_INSEPARABLE) {
    const chars = [...`${char} ${char}`];
    assert.equal(composeTextLines(chars, 2, (a, b) => b - a).length, 2);
    assert.equal(composeTextLines(chars, 2, (a, b) => b - a, policy).length, 2);
  }
  for (const text of [
    `日${'）'.repeat(12)}本`,
    `日${'（'.repeat(12)}本`,
    `日${'…'.repeat(12)}本`,
  ]) {
    const lines = composeTextLines([...text], 2, (a, b) => b - a, policy);
    assert.ok(lines.some((l) => l.overflow));
    assert.ok(lines.length <= 3);
  }
  assert.deepEqual(
    composeTextLines([...'（\n）'], 1, (a, b) => b - a, policy).map((l) => l.breakKind),
    ['hard', 'end'],
  );
});
test('vertical fitting shares exact glyph advances, spacing, source IDs and right-to-left columns', () => {
  const text = '日本「語、」ぁー。ABC12日本';
  for (const alignment of ['start', 'center', 'end'])
    for (const letterSpacing of [-2, 0, 3]) {
      const options = {
          writingMode: 'vertical-rl',
          alignment,
          letterSpacing,
          leading: { mode: 'manual', value: 55 },
        },
        result = layout(text, { ...options, textWrap: basicTextWrap('vertical-rl', 130) });
      assert.ok(result.lines.length > 1);
      assert.deepEqual(
        result.characters.map((c) => c.sourceIndex),
        [...text].map((_, i) => i),
      );
      assert.deepEqual(
        result.characters.map((c) => c.id),
        [...text].map((_, i) => `c${i + 1}`),
      );
      for (const [index, span] of result.lines.entries()) {
        const part = [...text].slice(span.sourceStart, span.fitEnd).join(''),
          expected = layout(part, options),
          actual = result.characters.filter(
            (c) => c.sourceIndex >= span.sourceStart && c.sourceIndex < span.fitEnd,
          );
        near(result.advances[index], expected.advances[0]);
        actual.forEach((c, i) => {
          near(c.base[0], expected.characters[i].base[0] - index * 55);
          near(c.base[1], expected.characters[i].base[1]);
        });
      }
    }
  const record = setCharacterStyles(text, undefined, {
      c1: { height: 1.5, width: 0.8 },
      c2: { size: 50, tracking: 4 },
    }),
    mixed = layout(text, {
      writingMode: 'vertical-rl',
      characterPositions: record,
      leading: { mode: 'auto' },
      textWrap: basicTextWrap('vertical-rl', 130),
    });
  assert.ok(mixed.lines.length > 1);
  near(mixed.characters[0].height, 1.5);
  for (const span of mixed.lines) {
    const entries = mixed.characters.filter(
      (c) => c.sourceIndex >= span.sourceStart && c.sourceIndex < span.fitEnd,
    );
    if (entries.length)
      near(
        span.advance,
        entries.at(-1).base[1] -
          entries[0].base[1] +
          layout(entries.at(-1).text, {
            writingMode: 'vertical-rl',
            size: entries.at(-1).size,
            characterPositions: setCharacterStyles(entries.at(-1).text, undefined, {
              c1: { height: entries.at(-1).height },
            }),
          }).advances[0],
      );
  }
});
test('basic/vertical wraps save at schema34, share Session history and export exact after reopen', () => {
  const f = scene(),
    before = serialize(f.session.snapshot());
  f.send('set-text-settings', {
    settings: {
      writingMode: 'vertical-rl',
      textWrap: basicTextWrap('vertical-rl', 130),
      leading: { mode: 'manual', value: 55 },
    },
  });
  const doc = f.session.snapshot(),
    saved = serialize(doc),
    svg = exportSVG(doc, { default: font });
  assert.equal(doc.schemaVersion, 34);
  const stale = structuredClone(doc);
  stale.schemaVersion = 33;
  assert.throws(() => validate(stale), /schemaVersion 34/);
  assert.equal(exportSVG(deserialize(saved), { default: font }), svg);
  assert.equal(f.session.historyView.past.length, 1);
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), before);
  f.send('redo');
  assert.equal(serialize(f.session.snapshot()), saved);
  assert.throws(() =>
    f.send('set-text-settings', { settings: { textWrap: { height: 0, policy } } }),
  );
  assert.equal(serialize(f.session.snapshot()), saved);
});
test('native line span validation refuses gaps, Unicode splits and forbidden soft breaks, preserving hard newlines', () => {
  const check = (text, rows) =>
    validateDesktopLineSpans({
      text,
      lineCount: rows.length,
      lineSpans: rows.map(([start, length, trailingWhitespaceLength = 0, newlineLength = 0]) => ({
        start,
        length,
        trailingWhitespaceLength,
        newlineLength,
      })),
    });
  check('日\n）', [
    [0, 2, 1, 1],
    [2, 1],
  ]);
  check('😀日', [
    [0, 2],
    [2, 1],
  ]);
  check('日\n', [
    [0, 2, 1, 1],
    [2, 0],
  ]);
  assert.throws(
    () =>
      check('日）', [
        [0, 1],
        [1, 1],
      ]),
    /violates/,
  );
  assert.throws(
    () =>
      check('（日', [
        [0, 1],
        [1, 1],
      ]),
    /violates/,
  );
  assert.throws(
    () =>
      check('……', [
        [0, 1],
        [1, 1],
      ]),
    /violates/,
  );
  assert.throws(
    () =>
      check('😀日', [
        [0, 1],
        [1, 2],
      ]),
    /Unicode/,
  );
  assert.throws(() =>
    check('日本', [
      [0, 1],
      [2, 0],
    ]),
  );
});
if (process.env.NECT_JP_WRAP_SMOKE_OUTPUT)
  test('real native horizontal/vertical snapshots retain exact geometry offline, through Undo and baking', () => {
    const rows = JSON.parse(fs.readFileSync(process.env.NECT_JP_WRAP_SMOKE_OUTPUT));
    assert.equal(rows.length, 34);
    for (const raw of rows) {
      const f = scene(raw),
        before = serialize(f.session.snapshot()),
        textWrap = basicTextWrap(raw.writingMode, raw.wrapWidth ?? raw.wrapHeight);
      f.send('set-os-font-text', {
        outline: raw,
        settings: { writingMode: raw.writingMode, textWrap },
      });
      const doc = f.session.snapshot(),
        saved = serialize(doc),
        svg = exportSVG(doc);
      assert.equal(doc.schemaVersion, 34);
      assert.equal(f.live().osFont.version, 5);
      assert.equal(evaluate(doc.layers[0], {}).error, null);
      assert.equal(exportSVG(deserialize(saved)), svg);
      f.send('undo');
      assert.equal(serialize(f.session.snapshot()), before);
      f.send('redo');
      assert.equal(serialize(f.session.snapshot()), saved);
      if (raw.points.length) {
        f.send('bake-text-outline');
        assert.equal(exportSVG(f.session.snapshot()), svg);
        f.send('undo');
      }
      const bad = structuredClone(raw);
      bad.lineSpans[0].length++;
      assert.throws(() =>
        f.send('set-os-font-text', {
          outline: bad,
          settings: { writingMode: raw.writingMode, textWrap },
        }),
      );
      assert.equal(serialize(f.session.snapshot()), saved);
    }
  });
