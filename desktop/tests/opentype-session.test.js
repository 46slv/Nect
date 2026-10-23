import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {
  blankDocument,
  node,
  connect,
  CommandSession,
  serialize,
  deserialize,
  evaluate,
  exportSVG,
  validate,
  setCharacterOffsets,
} from '../src/core.js';
import { parseFont } from '../src/fonts.js';
import { layoutText } from '../src/text-layout.js';
import { OPENTYPE_SCHEMA_VERSION } from '../src/opentype-features.js';

const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
const font = parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength));
const fonts = { default: font };
const options = { position: [40, 100], size: 64 };
const layout = (text, extra = {}) => layoutText(font, text, { ...options, ...extra });
function scene(activeFont = font) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    text = node('Text'),
    fill = node('Fill');
  text.text = 'office ffi AV';
  text.params.position.value = options.position;
  text.params.size.value = options.size;
  layer.nodes.push(text, fill);
  connect(layer, text.id, fill.id, 'path');
  connect(layer, fill.id, layer.output, 'path');
  const session = new CommandSession(doc, { fonts: () => ({ default: activeFont }) });
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: text.id, ...payload },
    });
  const set = (fontFeatures) => send('set-text-settings', { settings: { fontFeatures } });
  return { doc, layer, text, session, send, set };
}

test('real DejaVu liga changes glyphs and outlines; legacy and explicit Off retain exact geometry', () => {
  const legacy = layout('office ffi'),
    off = layout('office ffi', { fontFeatures: { liga: false } }),
    on = layout('office ffi', { fontFeatures: { liga: true } });
  assert.equal(legacy.glyphs.length, 10);
  assert.equal(on.glyphs.length, 6);
  assert.deepEqual(
    on.glyphs.map((g) => g.id),
    [82, 5044, 70, 72, 3, 5044],
  );
  assert.deepEqual(off.path.commands, legacy.path.commands);
  assert.notDeepEqual(on.path.commands, off.path.commands);
  assert.match(on.characterReason, /no verified one-to-one/);
  assert.deepEqual(on.characters, []);
  assert.equal(on.characterSuspended, false);
  const expected = font.getAdvanceWidth('office ffi', 64, {
    features: { liga: true, rlig: false },
  });
  assert.ok(Math.abs(on.advances[0] - expected) < 1e-7);
  const multi = layout('ffi\noffice', { fontFeatures: { liga: true }, alignment: 'center' });
  assert.equal(multi.glyphs.length, 5);
  assert.equal(multi.lines.length, 2);
});

test('liga and existing automatic kerning remain independent', () => {
  const kern = Object.create(font);
  kern.position = { getDefaultScriptName: () => 'latn', getKerningTables: () => null };
  kern.getKerningValue = () => -100;
  const metric = (extra) => layoutText(kern, 'AV ffi', { ...options, ...extra });
  for (const liga of [false, true]) {
    const on = metric({ fontFeatures: { liga }, kerning: true });
    const off = metric({ fontFeatures: { liga }, kerning: false });
    assert.ok(on.advances[0] < off.advances[0]);
    assert.deepEqual(
      on.glyphs.map((g) => g.id),
      off.glyphs.map((g) => g.id),
    );
  }
});

test('Session toggle promotes format once, Undo/Redo and JSON reopen retain exact SVG', () => {
  const f = scene(),
    before = serialize(f.session.snapshot()),
    originalSVG = exportSVG(f.session.snapshot(), fonts);
  f.set({ liga: true });
  const doc = f.session.snapshot(),
    saved = serialize(doc),
    svg = exportSVG(doc, fonts);
  assert.equal(doc.schemaVersion, OPENTYPE_SCHEMA_VERSION);
  assert.equal(f.session.historyView.past.length, 1);
  assert.notEqual(svg, originalSVG);
  assert.deepEqual(deserialize(saved), doc);
  const fresh = new CommandSession(deserialize(saved), { fonts: () => fonts });
  assert.equal(exportSVG(fresh.snapshot(), fonts), svg);
  assert.equal(
    Object.keys(evaluate(doc.layers[0], fonts).textLayout(f.text.id).glyphAdvances).length,
    9,
  );
  assert.ok(!svg.includes('<text') && !svg.includes('@font-face'));
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), before);
  f.send('redo');
  assert.equal(serialize(f.session.snapshot()), saved);
  f.set({ liga: false });
  assert.equal(exportSVG(f.session.snapshot(), fonts), originalSVG);
  f.set(null);
  assert.equal(
    f.session.snapshot().layers[0].nodes.find((n) => n.id === f.text.id).fontFeatures,
    undefined,
  );
  assert.equal(exportSVG(f.session.snapshot(), fonts), originalSVG);
});

test('import, generic duplicate and context deletion retain the promoted schema and one-step Undo', () => {
  const f = scene();
  f.set({ liga: true });
  const source = serialize(f.session.snapshot());
  const target = new CommandSession(blankDocument(), { fonts: () => fonts });
  const send = (type, payload = {}) => target.dispatch({ ...target.state, type, payload });
  const initial = serialize(target.snapshot());
  send('import-composition', { sourceText: source, compositionId: f.layer.id });
  assert.equal(target.snapshot().schemaVersion, OPENTYPE_SCHEMA_VERSION);
  const imported = serialize(target.snapshot());
  send('undo');
  assert.equal(serialize(target.snapshot()), initial);
  send('redo');
  assert.equal(serialize(target.snapshot()), imported);
  const copy = send('duplicate-layer', { layerId: f.layer.id });
  assert.equal(target.snapshot().schemaVersion, OPENTYPE_SCHEMA_VERSION);
  const duplicated = serialize(target.snapshot());
  send('delete-layers', { layerIds: [copy.layerId] });
  assert.equal(serialize(target.snapshot()), imported);
  send('undo');
  assert.equal(serialize(target.snapshot()), duplicated);
});

test('malformed and unsupported authored tags reject without a partial Session commit', () => {
  const f = scene(),
    before = serialize(f.session.snapshot()),
    state = f.session.state;
  for (const features of [
    { liga: 1 },
    { dlig: true },
    { onum: true },
    { liga: true, kern: false },
    {},
  ]) {
    assert.throws(() => f.set(features), /liga/);
    assert.equal(serialize(f.session.snapshot()), before);
    assert.deepEqual(f.session.state, state);
  }
  for (const schemaVersion of [24, 37]) {
    const doc = f.session.snapshot();
    doc.schemaVersion = schemaVersion;
    doc.layers[0].nodes.find((n) => n.id === f.text.id).fontFeatures = { liga: true };
    assert.throws(() => validate(doc), /schemaVersion/);
  }
});

test('unsupported font lookups reject before toggling; missing fonts never fall back', () => {
  const noLiga = Object.create(font);
  noLiga.tables = { ...font.tables, gsub: undefined };
  const f = scene(noLiga),
    before = serialize(f.session.snapshot());
  assert.throws(() => f.set({ liga: true }), /no supported Latin/);
  assert.equal(serialize(f.session.snapshot()), before);
  assert.throws(
    () =>
      layoutText({ nectVariable: {} }, 'ffi', {
        ...options,
        fontFeatures: { liga: true },
      }),
    /static/,
  );
});

test('incompatible later edits are atomic and retain the saved ligature setting', () => {
  const f = scene();
  f.set({ liga: true });
  const before = serialize(f.session.snapshot()),
    state = f.session.state;
  for (const settings of [{ writingMode: 'vertical-rl' }, { textWrap: { width: 200 } }]) {
    assert.throws(() => f.send('set-text-settings', { settings }), /whole static horizontal/);
    assert.equal(serialize(f.session.snapshot()), before);
  }
  assert.throws(
    () => f.send('set-text-content', { text: '\u0627\u0644' }),
    /whole static horizontal/,
  );
  assert.equal(serialize(f.session.snapshot()), before);
  assert.deepEqual(f.session.state, state);
  const withAdjustments = f.session.snapshot();
  withAdjustments.layers[0].nodes.find((n) => n.id === f.text.id).characterPositions =
    setCharacterOffsets(f.text.text, undefined, { c1: [3, 0] });
  assert.throws(() => validate(withAdjustments), /character adjustments/);
  f.set(null);
  f.send('set-text-settings', { settings: { writingMode: 'vertical-rl' } });
  assert.equal(
    f.session.snapshot().layers[0].nodes.find((n) => n.id === f.text.id).writingMode,
    'vertical-rl',
  );
});
