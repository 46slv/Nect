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
  pathBounds,
} from '../src/core.js';
import { desktopTextIntent, normalizeDesktopText } from '../src/desktop-font-text.js';
const face = {
  family: 'Test face',
  name: 'Regular',
  weight: 400,
  style: 0,
  stretch: 5,
  format: 1,
  color: false,
};
const raw = (n, settings = {}) => ({
  ...structuredClone(desktopTextIntent(n, face, settings)),
  engine: 'directwrite-v1',
  ascent: 80,
  advanceWidth: 20,
  lineCount: 2,
  glyphCount: 8,
  points: [
    [0, 0],
    [20, 0],
    [20, 10],
    [0, 10],
  ],
  types: [0, 1, 1, 129],
});
function fixture(outline) {
  const doc = document(),
    layer = doc.layers[0],
    n = node('Text'),
    fill = node('Fill');
  n.text = outline?.text ?? '日本語\nمرحبا';
  if (outline) {
    n.params.size.value = outline.size;
    n.params.letterSpacing.value = outline.tracking;
    n.params.lineSpacing.value = outline.lineSpacing;
    n.kerning = outline.kerning;
    n.alignment = ['start', 'center', 'end'][outline.alignment];
  }
  fill.params.path.source = n.id;
  layer.nodes = [n, fill, layer.nodes.find((n) => n.id === layer.output)];
  layer.nodes[2].params.path.source = fill.id;
  const s = new CommandSession(doc),
    send = (type, payload = {}) =>
      s.dispatch({ ...s.state, type, payload: { layerId: layer.id, nodeId: n.id, ...payload } });
  return { n, s, send, live: () => s.snapshot().layers[0].nodes[0] };
}
test('shaped Text codec renders, exports, bakes identically and undoes without a font service', () => {
  const f = fixture(),
    before = serialize(f.s.snapshot());
  f.send('set-os-font-text', { outline: raw(f.n) });
  assert.equal(f.s.snapshot().schemaVersion, 29);
  assert.equal(f.live().type, 'Text');
  const retained = serialize(f.s.snapshot()),
    loaded = deserialize(retained),
    a = evaluate(loaded.layers[0], {});
  assert.equal(a.error, null);
  assert.match(exportSVG(loaded), /<path/);
  assert.equal(Object.keys(loaded.fontAssets || {}).length, 0);
  f.send('bake-text-outline');
  assert.deepEqual(pathBounds(evaluate(f.s.snapshot().layers[0], {}).path), pathBounds(a.path));
  f.send('undo');
  assert.equal(serialize(f.s.snapshot()), retained);
  f.send('undo');
  assert.equal(serialize(f.s.snapshot()), before);
});
test('text and typography snapshots change atomically; stale, locked, driven and malformed edits refuse', () => {
  const f = fixture();
  f.send('set-os-font-text', { outline: raw(f.n) });
  const before = serialize(f.s.snapshot());
  const settings = {
      size: 80,
      tracking: 3,
      lineSpacing: 2,
      kerning: false,
      ligatures: false,
      alignment: 'center',
    },
    text = '日本語\n新しい Text';
  f.send('set-os-font-text', { text, settings, outline: raw(f.live(), { text, ...settings }) });
  assert.equal(f.live().params.letterSpacing.value, 3);
  assert.equal(f.live().osFont.outline.ligatures, false);
  assert.equal(evaluate(f.s.snapshot().layers[0], {}).error, null);
  f.send('undo');
  assert.equal(serialize(f.s.snapshot()), before);
  for (const change of [
    (r) => r.size++,
    (r) => (r.text += 'x'),
    (r) => (r.face.weight = 1001),
    (r) => (r.points[0][0] = Infinity),
    (r) => (r.extra = 1),
    (r) => r.types.pop(),
  ]) {
    const value = raw(f.live());
    change(value);
    assert.throws(() => f.send('set-os-font-text', { outline: value }));
    assert.equal(serialize(f.s.snapshot()), before);
  }
  assert.throws(() => f.send('set-text-content', { text: 'unprepared' }));
  assert.equal(serialize(f.s.snapshot()), before);
  const locked = deserialize(before);
  locked.layers[0].nodes[0].params.size.locked = true;
  const session = new CommandSession(locked),
    saved = serialize(session.snapshot());
  assert.throws(() =>
    session.dispatch({
      ...session.state,
      type: 'set-os-font-text',
      payload: {
        layerId: locked.layers[0].id,
        nodeId: f.n.id,
        settings: { size: 80 },
        outline: raw(f.live(), { size: 80 }),
      },
    }),
  );
  assert.equal(serialize(session.snapshot()), saved);
});
test('Unicode source normalization preserves complex scripts and refuses invalid/control data', () => {
  assert.equal(normalizeDesktopText('e\u0301\r\nمرحبا\nשלום\n日本語'), 'é\nمرحبا\nשלום\n日本語');
  for (const text of ['\ud800', 'x\u0000', '\uffff', 'x'.repeat(4097), '\n'.repeat(257)])
    assert.throws(() => normalizeDesktopText(text));
});
test('direct Windows path conversion is atomic and refuses driven positions', () => {
  const f = fixture(),
    before = serialize(f.s.snapshot());
  f.send('bake-os-text-outline', { outline: raw(f.n) });
  assert.equal(f.live().type, 'Multi Path');
  f.send('undo');
  assert.equal(serialize(f.s.snapshot()), before);
  const driven = fixture();
  driven.n.params.position.expression = '[10,20]';
  const doc = driven.s.snapshot();
  doc.layers[0].nodes[0].params.position.expression = '10;20';
  const session = new CommandSession(doc),
    saved = serialize(session.snapshot());
  assert.throws(() =>
    session.dispatch({
      ...session.state,
      type: 'bake-os-text-outline',
      payload: { layerId: doc.layers[0].id, nodeId: driven.n.id, outline: raw(driven.n) },
    }),
  );
  assert.equal(serialize(session.snapshot()), saved);
});
if (process.env.NECT_DESKTOP_FONT_SMOKE_OUTPUT)
  test('actual Windows geometry survives the pure codec/evaluator and path conversion', () => {
    const outlines = JSON.parse(
      fs.readFileSync(process.env.NECT_DESKTOP_FONT_SMOKE_OUTPUT, 'utf8'),
    );
    for (const outline of outlines) {
      const f = fixture(outline);
      f.send('set-os-font-text', { outline });
      const loaded = deserialize(serialize(f.s.snapshot()));
      const a = evaluate(loaded.layers[0], {});
      assert.equal(a.error, null, outline.face.family);
      const svg = exportSVG(loaded);
      f.send('bake-text-outline');
      assert.equal(exportSVG(f.s.snapshot()), svg, outline.face.family);
    }
  });
