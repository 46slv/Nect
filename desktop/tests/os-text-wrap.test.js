import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {
  CommandSession,
  document,
  node,
  serialize,
  deserialize,
  evaluate,
  exportSVG,
} from '../src/core.js';
import { desktopTextIntent } from '../src/desktop-font-text.js';
const face = {
  family: 'Test face',
  name: 'Regular',
  weight: 400,
  style: 0,
  stretch: 5,
  format: 1,
  color: false,
};
function fixture(raw) {
  const doc = document(),
    l = doc.layers[0],
    n = node('Text'),
    fill = node('Fill');
  n.text = raw?.text ?? 'Wrapped text';
  n.params.position.value = [80, 80];
  n.params.size.value = raw?.size ?? 32;
  n.params.letterSpacing.value = raw?.tracking ?? 0;
  n.params.lineSpacing.value = raw?.lineSpacing ?? 1.2;
  n.alignment = ['start', 'center', 'end'][raw?.alignment ?? 0];
  fill.params.path.source = n.id;
  l.nodes = [n, fill, l.nodes.find((n) => n.id === l.output)];
  l.nodes[2].params.path.source = fill.id;
  const session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: l.id, nodeId: n.id, ...payload },
      });
  return { session, send, n, l, live: () => session.snapshot().layers[0].nodes[0] };
}
const raw = (n, width = 160) => ({
  ...desktopTextIntent(n, face, { textWrap: { width } }),
  engine: 'directwrite-v3',
  axes: [],
  wrapOverflow: false,
  advanceHeight: 80,
  ascent: 25,
  advanceWidth: 100,
  lineCount: 2,
  glyphCount: 10,
  points: [
    [0, 0],
    [100, 0],
    [100, 20],
    [0, 20],
  ],
  types: [0, 1, 1, 129],
});
test('prepared wrap is atomic, editable schema32 and remains portable without a font service', () => {
  const f = fixture(),
    before = serialize(f.session.snapshot());
  f.send('set-os-font-text', { outline: raw(f.n), settings: { textWrap: { width: 160 } } });
  assert.equal(f.session.snapshot().schemaVersion, 32);
  assert.equal(f.live().osFont.version, 4);
  assert.deepEqual(f.live().textWrap, { width: 160 });
  assert.equal(f.session.historyView.past.length, 1);
  const saved = serialize(f.session.snapshot()),
    doc = deserialize(saved),
    svg = exportSVG(doc);
  assert.equal(evaluate(doc.layers[0], {}).error, null);
  assert.equal(Object.keys(doc.fontAssets || {}).length, 0);
  f.send('bake-text-outline');
  assert.equal(exportSVG(f.session.snapshot()), svg);
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), saved);
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), before);
});
test('unprepared width, vertical wrap, stale/locked settings and mismatched metadata preserve last snapshot', () => {
  const f = fixture();
  f.send('set-os-font-text', { outline: raw(f.n), settings: { textWrap: { width: 160 } } });
  const saved = serialize(f.session.snapshot());
  for (const payload of [
    { settings: { textWrap: { width: 80 } } },
    { settings: { writingMode: 'vertical-rl' } },
  ])
    assert.throws(() => f.send('set-text-settings', payload));
  for (const patch of [
    { wrapWidth: 80 },
    { wrapOverflow: true },
    { engine: 'directwrite-v2' },
    { lineCount: 257 },
  ])
    assert.throws(() =>
      f.send('set-os-font-text', {
        outline: { ...raw(f.live()), ...patch },
        settings: { textWrap: { width: 160 } },
      }),
    );
  assert.equal(serialize(f.session.snapshot()), saved);
  const older = deserialize(saved);
  older.schemaVersion = 31;
  assert.throws(() => deserialize(serialize(older)));
  f.send('set-parameter-lock', { key: 'size', locked: true });
  const locked = serialize(f.session.snapshot());
  assert.throws(
    () =>
      f.send('set-os-font-text', {
        outline: raw(f.live()),
        settings: { size: 48, textWrap: { width: 160 } },
      }),
    /locked/,
  );
  assert.equal(serialize(f.session.snapshot()), locked);
});
if (process.env.NECT_OS_WRAP_SMOKE_OUTPUT)
  test('actual Windows wrapping snapshots preserve settings, geometry and overflow through save/bake/Undo', () => {
    const rows = JSON.parse(fs.readFileSync(process.env.NECT_OS_WRAP_SMOKE_OUTPUT, 'utf8'));
    for (const outline of rows.filter((r) => r.engine === 'directwrite-v3')) {
      const f = fixture(outline);
      f.send('set-os-font-text', { outline, settings: { textWrap: { width: outline.wrapWidth } } });
      const saved = serialize(f.session.snapshot()),
        doc = deserialize(saved),
        svg = exportSVG(doc);
      assert.equal(evaluate(doc.layers[0], {}).error, null);
      assert.equal(!!evaluate(doc.layers[0], {}).warnings.length, outline.wrapOverflow);
      if (outline.points.length) {
        f.send('bake-text-outline');
        assert.equal(exportSVG(f.session.snapshot()), svg);
        f.send('undo');
        assert.equal(serialize(f.session.snapshot()), saved);
      }
    }
    const fraction = rows.find((r) => Math.fround(r.wrapWidth) === Math.fround(160.123456789)),
      fractionCase = fixture(fraction);
    fractionCase.send('set-os-font-text', {
      outline: fraction,
      settings: { textWrap: { width: 160.123456789 } },
    });
    assert.equal(fractionCase.live().textWrap.width, 160.123456789);
    assert.equal(evaluate(fractionCase.session.snapshot().layers[0], {}).error, null);
    const [unwrapped, wide, narrow] = rows,
      f = fixture(unwrapped);
    f.send('set-os-font-text', { outline: unwrapped });
    const before = serialize(f.session.snapshot());
    f.send('set-os-font-text', {
      outline: wide,
      settings: { textWrap: { width: wide.wrapWidth } },
    });
    const wrapped = serialize(f.session.snapshot());
    f.send('set-os-font-text', {
      outline: narrow,
      settings: { textWrap: { width: narrow.wrapWidth } },
    });
    assert.equal(f.live().textWrap.width, narrow.wrapWidth);
    f.send('undo');
    assert.equal(serialize(f.session.snapshot()), wrapped);
    f.send('set-os-font-text', { outline: unwrapped, settings: { textWrap: null } });
    assert.equal(f.live().textWrap, undefined);
    f.send('undo');
    assert.equal(serialize(f.session.snapshot()), wrapped);
    f.send('undo');
    assert.equal(serialize(f.session.snapshot()), before);
  });
