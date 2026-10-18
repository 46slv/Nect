import test from 'node:test';
import assert from 'node:assert/strict';
import {
  CommandSession,
  document,
  node,
  serialize,
  deserialize,
  evaluate,
  exportSVG,
  pathBounds,
  createCharacterPositions,
} from '../src/core.js';
const raw = (text = 'ABC', family = 'Available OS font') => ({
  family,
  style: 0,
  text: text.normalize('NFC'),
  size: 100,
  ascent: 80,
  points: [
    [0, 0],
    [10, 0],
    [10, 10],
    [0, 10],
  ],
  types: [0, 1, 1, 161],
});
function fixture() {
  const doc = document(),
    layer = doc.layers[0],
    text = node('Text'),
    fill = node('Fill'),
    output = layer.nodes.find((n) => n.id === layer.output);
  text.text = 'ABC';
  fill.params.path.source = text.id;
  output.params.path.source = fill.id;
  layer.nodes = [text, fill, output];
  const session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: layer.id, nodeId: text.id, ...payload },
      });
  return {
    doc,
    layer,
    text,
    session,
    send,
    live: () => session.snapshot().layers[0].nodes.find((n) => n.id === text.id),
  };
}
test('OS font stays Text; geometry-only schema 28 renders, exports and reloads with zero OS/font services', () => {
  const f = fixture(),
    before = serialize(f.session.snapshot());
  f.send('set-os-font-text', { outline: raw() });
  const after = serialize(f.session.snapshot());
  assert.equal(f.live().type, 'Text');
  assert.equal(f.live().osFont.family, 'Available OS font');
  assert.equal(f.session.snapshot().schemaVersion, 28);
  assert.equal(f.session.historyView.past.length, 1);
  assert.equal(Object.keys(f.session.snapshot().fontAssets || {}).length, 0);
  const reloaded = deserialize(after),
    result = evaluate(reloaded.layers[0], {});
  assert.equal(result.error, null);
  assert.deepEqual(pathBounds(result.path), [
    [200, 205, 210],
    [220, 225, 230],
  ]);
  assert.match(exportSVG(reloaded), /<path/);
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), before);
  f.send('redo');
  assert.equal(serialize(f.session.snapshot()), after);
});
test('prepared text changes preserve OS reference and one Undo restores text and snapshot together', () => {
  const f = fixture();
  f.send('set-os-font-text', { outline: raw() });
  const before = serialize(f.session.snapshot());
  f.send('set-os-font-text', { text: '日本語', outline: raw('日本語') });
  assert.equal(f.live().text, '日本語');
  assert.equal(f.live().osFont.family, 'Available OS font');
  assert.equal(f.session.historyView.past.length, 2);
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), before);
  f.send('redo');
  assert.equal(f.live().text, '日本語');
});
test('unprepared, malformed, stale and locked edits never lose the last drawable snapshot', () => {
  const f = fixture();
  f.send('set-os-font-text', { outline: raw() });
  const before = serialize(f.session.snapshot());
  assert.throws(() => f.send('set-text-content', { text: 'Unprepared change' }));
  assert.throws(() => f.send('set-os-font-text', { text: 'New', outline: raw('Different') }));
  assert.equal(serialize(f.session.snapshot()), before);
  const old = f.session.state;
  f.send('rename', { name: 'New name' });
  const current = serialize(f.session.snapshot());
  assert.throws(() =>
    f.session.dispatch({
      ...old,
      type: 'set-os-font-text',
      payload: { layerId: f.layer.id, nodeId: f.text.id, text: 'New', outline: raw('New') },
    }),
  );
  assert.equal(serialize(f.session.snapshot()), current);
  for (const mutate of [
    (d) => {
      d.schemaVersion = 27;
    },
    (d) => {
      d.layers[0].nodes[0].osFont.outline.fontBytes = 'forbidden';
    },
    (d) => {
      d.layers[0].nodes[0].osFont.outline.points[0][0] = Infinity;
    },
  ]) {
    const d = f.session.snapshot();
    mutate(d);
    assert.throws(() => deserialize(serialize(d)));
  }
});
test('cached OS Text permits position/size changes and conversion to paths while font is missing', () => {
  const f = fixture();
  f.send('set-os-font-text', { outline: raw() });
  f.send('set-parameter', { nodeId: f.text.id, key: 'size', value: 200 });
  const result = evaluate(f.session.snapshot().layers[0], {});
  assert.equal(result.error, null);
  assert.deepEqual(pathBounds(result.path), [
    [200, 210, 220],
    [140, 150, 160],
  ]);
  f.send('bake-text-outline');
  assert.equal(f.live().type, 'Multi Path');
  assert.equal(f.live().osFont, undefined);
  f.send('undo');
  assert.equal(f.live().type, 'Text');
  f.send('clear-os-font-text');
  assert.equal(f.live().osFont, undefined);
  assert.equal(f.live().font, 'default');
});
test('existing character identity maps survive a supported OS text edit', () => {
  const f = fixture(),
    doc = f.doc;
  doc.layers[0].nodes[0].characterPositions = createCharacterPositions('ABC');
  const s = new CommandSession(doc);
  const send = (text) =>
    s.dispatch({
      ...s.state,
      type: 'set-os-font-text',
      payload: { layerId: f.layer.id, nodeId: f.text.id, text, outline: raw(text) },
    });
  send('ABC');
  send('ABCD');
  assert.equal(s.snapshot().layers[0].nodes[0].text, 'ABCD');
});
test('OS Text can become empty without losing its font reference or substituting glyphs', () => {
  const f = fixture();
  f.send('set-os-font-text', { outline: raw() });
  const before = serialize(f.session.snapshot());
  f.send('set-os-font-text', { text: '', outline: { ...raw(''), points: [], types: [] } });
  assert.equal(f.live().text, '');
  assert.ok(f.live().osFont);
  assert.equal(evaluate(f.session.snapshot().layers[0], {}).error, null);
  assert.equal(evaluate(f.session.snapshot().layers[0], {}).path.contours.length, 0);
  assert.doesNotThrow(() => deserialize(serialize(f.session.snapshot())));
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), before);
});
