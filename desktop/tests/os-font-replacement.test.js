import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { makeFontAsset, parseFont, resolveFonts } from '../src/fonts.js';
import {
  CommandSession,
  document,
  node,
  createCharacterPositions,
  serialize,
  deserialize,
  evaluate,
  exportSVG,
} from '../src/core.js';
import { desktopTextIntent } from '../src/desktop-font-text.js';
import { layoutText } from '../src/text-layout.js';

const bytes = (name = 'NectJPFixture-Regular.otf') => {
  const b = fs.readFileSync(new URL(`./fixtures/fonts/${name}`, import.meta.url));
  return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
};
const defaultBytes = () => {
  const b = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
  return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
};
const outline = (text) => ({
  family: 'Saved OS face',
  style: 0,
  text: text.normalize('NFC'),
  size: 100,
  ascent: 80,
  points: [
    [0, 0],
    [20, 0],
    [20, 10],
    [0, 10],
  ],
  types: [0, 1, 1, 129],
});
function fixture(text = '日本語', { modern = false, shaped = false, ligatures = true } = {}) {
  const doc = document(),
    layer = doc.layers[0],
    n = node('Text'),
    fill = node('Fill'),
    output = layer.nodes.find((n) => n.id === layer.output);
  n.text = text;
  n.params.size.value = 100;
  n.characterPositions = createCharacterPositions(text);
  fill.params.path.source = n.id;
  output.params.path.source = fill.id;
  layer.nodes = [n, fill, output];
  if (modern) n.writingMode = 'vertical-rl';
  const s = new CommandSession(doc),
    send = (type, payload = {}) =>
      s.dispatch({ ...s.state, type, payload: { layerId: layer.id, nodeId: n.id, ...payload } });
  let raw = outline(text);
  if (modern || shaped || !ligatures) {
    const face = {
      family: 'Saved OS face',
      name: 'Regular',
      weight: 400,
      style: 0,
      stretch: 5,
      format: 1,
      color: false,
    };
    raw = {
      ...desktopTextIntent(n, face, { axes: {}, ligatures }),
      engine: 'directwrite-v2',
      axes: [],
      ascent: 80,
      advanceWidth: 20,
      advanceHeight: 300,
      lineCount: text.split('\n').length,
      glyphCount: [...text].length,
      points: raw.points,
      types: raw.types,
    };
  }
  send('set-os-font-text', { outline: raw });
  return { s, send, layer, n, live: () => s.snapshot().layers[0].nodes[0] };
}
function unchanged(f, operation, pattern) {
  const before = serialize(f.s.snapshot()),
    revision = f.s.state.expectedRevision,
    history = f.s.historyView.past.length,
    svg = exportSVG(f.s.snapshot());
  assert.throws(operation, pattern);
  assert.equal(serialize(f.s.snapshot()), before);
  assert.equal(f.s.state.expectedRevision, revision);
  assert.equal(f.s.historyView.past.length, history);
  assert.equal(exportSVG(deserialize(before)), svg);
}

test('Japanese OS Text adopts chosen bytes atomically without an intermediate default face', () => {
  const f = fixture(),
    before = serialize(f.s.snapshot()),
    original = f.live(),
    count = f.s.historyView.past.length;
  unchanged(f, () => f.send('clear-os-font-text'), /OS font retained/);
  const result = f.send('replace-os-text-font', { bytes: bytes(), fileName: 'fixture.otf' });
  assert.equal(result.nodeId, original.id);
  assert.equal(result.fontId, f.live().font);
  assert.equal(f.live().type, 'Text');
  assert.equal(f.live().osFont, undefined);
  const expected = structuredClone(original);
  delete expected.osFont;
  expected.font = result.fontId;
  assert.deepEqual(f.live(), expected);
  assert.equal(f.s.historyView.past.length, count + 1);
  assert.equal(Object.keys(f.s.snapshot().fontAssets).length, 1);
  assert.deepEqual(f.s.snapshot().layers[0].nodes.slice(1), f.layer.nodes.slice(1));
  const after = serialize(f.s.snapshot()),
    svg = exportSVG(f.s.snapshot());
  assert.equal(evaluate(f.s.snapshot().layers[0], resolveFonts(f.s.snapshot())).error, null);
  assert.equal(exportSVG(deserialize(after)), svg);
  f.send('undo');
  assert.equal(serialize(f.s.snapshot()), before);
  f.send('redo');
  assert.equal(serialize(f.s.snapshot()), after);
});

test('modern multiline vertical layout and stable character identities survive replacement', () => {
  const f = fixture('日本\n語ABC', { modern: true, ligatures: false }),
    original = f.live();
  f.send('replace-os-text-font', { bytes: bytes(), fileName: 'fixture.otf' });
  assert.equal(f.live().text, original.text);
  assert.equal(f.live().writingMode, 'vertical-rl');
  assert.deepEqual(f.live().params, original.params);
  assert.deepEqual(f.live().characterPositions, original.characterPositions);
  assert.equal(evaluate(f.s.snapshot().layers[0], resolveFonts(f.s.snapshot())).error, null);
  assert.equal(exportSVG(deserialize(serialize(f.s.snapshot()))), exportSVG(f.s.snapshot()));
});

test('missing coverage, malformed bytes and restricted embedding retain the complete OS snapshot', () => {
  const f = fixture('世界の麺'); // The licensed fixture intentionally has no の glyph.
  unchanged(
    f,
    () => f.send('replace-os-text-font', { bytes: bytes(), fileName: 'fixture.otf' }),
    /Missing glyphs/,
  );
  unchanged(f, () => f.send('replace-os-text-font', { bytes: new ArrayBuffer(20) }), /TTF|font/i);
  unchanged(f, () => f.send('replace-os-text-font', { fontId: 'font-absent' }), /Missing replacement/);
  unchanged(f, () => f.send('replace-os-text-font', { fontId: 'default' }), /OS font retained/);
  const restricted = bytes(),
    view = new DataView(restricted);
  let patched = false;
  for (let i = 0; i < view.getUint16(4); i++) {
    const at = 12 + i * 16;
    if (String.fromCharCode(...new Uint8Array(restricted, at, 4)) === 'OS/2') {
      view.setUint16(view.getUint32(at + 8) + 8, 2);
      patched = true;
    }
  }
  assert.ok(patched);
  unchanged(f, () => f.send('replace-os-text-font', { bytes: restricted }), /restricts/);
});

test('existing static asset selection reuses exact identity, without embedding a duplicate', () => {
  const f = fixture(),
    doc = f.s.snapshot(),
    asset = makeFontAsset(bytes(), 'fixture.otf');
  doc.fontAssets = { [asset.id]: asset.asset };
  const s = new CommandSession(doc),
    send = (payload) =>
      s.dispatch({
        ...s.state,
        type: 'replace-os-text-font',
        payload: { layerId: f.layer.id, nodeId: f.n.id, ...payload },
      });
  send({ bytes: bytes(), fileName: 'same-font-other-name.otf' });
  assert.equal(s.snapshot().layers[0].nodes[0].font, asset.id);
  assert.equal(Object.keys(s.snapshot().fontAssets).length, 1);
  s.dispatch({ ...s.state, type: 'undo' });
  send({ fontId: asset.id });
  assert.equal(s.snapshot().layers[0].nodes[0].font, asset.id);
  assert.equal(Object.keys(s.snapshot().fontAssets).length, 1);
});

test('font choice payloads, locks, stale revisions and active drafts cannot partially replace Text', () => {
  const f = fixture();
  for (const payload of [
    {},
    { bytes: bytes(), fontId: 'default' },
    { bytes: new Uint8Array(20) },
    { bytes: bytes(), characterIds: [] },
  ])
    unchanged(f, () => f.send('replace-os-text-font', payload));
  const old = f.s.state;
  f.send('rename', { name: 'New name' });
  unchanged(f, () =>
    f.s.dispatch({
      ...old,
      type: 'replace-os-text-font',
      payload: { layerId: f.layer.id, nodeId: f.n.id, bytes: bytes() },
    }),
  );
  for (const mutate of [
    (d) => {
      d.layers[0].locked = true;
    },
    (d) => {
      d.layers[0].nodes[0].locked = true;
    },
    (d) => {
      d.layers[0].nodes[0].bypassed = true;
    },
  ]) {
    const doc = f.s.snapshot();
    mutate(doc);
    const s = new CommandSession(doc),
      before = serialize(s.snapshot());
    assert.throws(() =>
      s.dispatch({
        ...s.state,
        type: 'replace-os-text-font',
        payload: { layerId: f.layer.id, nodeId: f.n.id, bytes: bytes() },
      }),
    );
    assert.equal(serialize(s.snapshot()), before);
  }
  f.send('begin-text-draft');
  unchanged(f, () => f.send('replace-os-text-font', { bytes: bytes() }), /draft/i);
});

test('saved Windows ligature-off retains the static default; unsupported ligature-on refuses', () => {
  const f = fixture('ABC', { ligatures: false });
  f.send('replace-os-text-font', { bytes: bytes(), fileName: 'fixture.otf' });
  assert.equal(f.live().fontFeatures, undefined);
  assert.equal(exportSVG(deserialize(serialize(f.s.snapshot()))), exportSVG(f.s.snapshot()));
  const unsupported = fixture('日本語', { modern: true, ligatures: true });
  unchanged(
    unsupported,
    () => unsupported.send('replace-os-text-font', { bytes: bytes() }),
    /ligatures|static/i,
  );
  const unsupportedFace = fixture('ABC', { shaped: true, ligatures: true });
  unchanged(
    unsupportedFace,
    () => unsupportedFace.send('replace-os-text-font', { bytes: bytes() }),
    /no supported Latin.*liga/i,
  );
});

test('Windows ligatures-on becomes explicit static intent, not the differing legacy default', () => {
  const font = parseFont(defaultBytes()),
    options = { position: [0, 0], size: 100 },
    normal = layoutText(font, 'office ffi', options),
    explicit = layoutText(font, 'office ffi', { ...options, fontFeatures: { liga: true } }),
    disabled = layoutText(font, 'office ffi', { ...options, fontFeatures: { liga: false } });
  assert.equal(normal.path.toPathData(5), disabled.path.toPathData(5));
  assert.notEqual(normal.path.toPathData(5), explicit.path.toPathData(5));
  const f = fixture('office ffi', { shaped: true, ligatures: true }),
    doc = f.s.snapshot(),
    s = new CommandSession(doc, { fonts: () => ({ default: font }) });
  s.dispatch({
    ...s.state,
    type: 'replace-os-text-font',
    payload: { layerId: f.layer.id, nodeId: f.n.id, fontId: 'default' },
  });
  assert.deepEqual(s.snapshot().layers[0].nodes[0].fontFeatures, { liga: true });
  assert.ok(s.snapshot().schemaVersion >= 38);
  assert.equal(s.snapshot().layers[0].nodes[0].font, 'default');
  assert.equal(evaluate(s.snapshot().layers[0], { default: font }).error, null);
});
