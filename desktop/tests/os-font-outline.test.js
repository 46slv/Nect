import test from 'node:test';
import assert from 'node:assert/strict';
import {
  CommandSession,
  document,
  node,
  serialize,
  deserialize,
  evaluate,
  pathBounds,
} from '../src/core.js';
import { osTextIntent, osOutlineGeometry, normalizeOsText } from '../src/os-font-outline.js';
test('OS Unicode policy normalizes safe composed text and explicitly refuses complex/hidden sequences', () => {
  for (const text of [
    '日本語かなカナ漢字 ABC １２３',
    'Ελληνικά',
    'Русский',
    '한글',
    'Cafe\u0301',
    'か\u3099',
  ])
    assert.equal(normalizeOsText(text), text.normalize('NFC'));
  for (const text of [
    'a\nb',
    'مرحبا',
    'עברית',
    'नमस्ते',
    'ภาษาไทย',
    'a\u200db',
    'x\u0301',
    '😀',
    '☺\ufe0f',
    'ｶﾞ',
    '\ud800',
  ])
    assert.throws(() => normalizeOsText(text), /Unsupported|one nonempty/);
});

function fixture() {
  const doc = document(),
    layer = doc.layers[0],
    text = node('Text'),
    fill = node('Fill');
  text.text = 'DIN';
  fill.params.path.source = text.id;
  layer.nodes.push(text, fill);
  layer.nodes.find((n) => n.id === layer.output).params.path.source = fill.id;
  const raw = {
    family: 'DIN Alternate',
    style: 0,
    text: text.text,
    size: 100,
    ascent: 80,
    points: [
      [1, 2],
      [11, 2],
      [11, 12],
      [1, 12],
      [3, 4],
      [3, 10],
      [9, 10],
      [9, 4],
    ],
    types: [0, 1, 1, 129, 0, 1, 1, 161],
  };
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: text.id, ...payload },
    });
  return { doc, layer, text, raw, session, send };
}

test('OS geometry replaces Text through one Session Undo; holes, links, IDs and portable codec remain intact', () => {
  const f = fixture(),
    before = serialize(f.session.snapshot());
  f.send('bake-os-text-outline', { outline: f.raw });
  const after = serialize(f.session.snapshot()),
    baked = f.session.snapshot().layers[0].nodes.find((n) => n.id === f.text.id);
  assert.equal(baked.type, 'Multi Path');
  assert.equal(baked.contours.length, 2);
  assert.equal(baked.units.length, 1);
  assert.equal(baked.units[0].contours.length, 2);
  assert.equal(baked.contours[0].group, baked.contours[1].group);
  const rendered = evaluate(deserialize(after).layers[0]);
  assert.equal(rendered.error, null);
  assert.deepEqual(pathBounds(rendered.path), [
    [200, 205, 210],
    [222, 227, 232],
  ]);
  assert.equal(f.session.historyView.past.length, 1);
  assert.ok(!after.includes('DIN Alternate'));
  assert.equal(Object.keys(f.session.snapshot().fontAssets || {}).length, 0);
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), before);
  f.send('redo');
  assert.equal(serialize(f.session.snapshot()), after);
});

test('native cubic control points and explicit closing endpoint become exact relative handles', () => {
  const f = fixture();
  const raw = {
    ...f.raw,
    points: [
      [0, 0],
      [10, 0],
      [10, 10],
      [0, 10],
      [-10, 10],
      [-10, 0],
      [0, 0],
    ],
    types: [0, 3, 3, 3, 3, 3, 131],
  };
  let id = 0;
  const path = osOutlineGeometry(raw, f.text, () => `test-${++id}`);
  assert.equal(path.contours[0].points.length, 2);
  const [a, b] = path.contours[0].points;
  assert.deepEqual(a.handleA, [-10, 0]);
  assert.deepEqual(a.handleB, [10, 0]);
  assert.deepEqual(b.handleA, [10, 0]);
  assert.deepEqual(b.handleB, [-10, 0]);
});

test('malformed, oversized, stale and unsupported native results fail without authored or history mutation', () => {
  for (const change of [
    (r) => {
      r.text = 'stale';
    },
    (r) => {
      r.size = 101;
    },
    (r) => {
      r.size = '100';
    },
    (r) => {
      r.points[0][0] = NaN;
    },
    (r) => {
      r.types[1] = 3;
    },
    (r) => {
      r.types[3] = 1;
    },
    (r) => {
      r.types[0] = 128;
    },
    (r) => {
      r.style = 4;
    },
    (r) => {
      r.ascent = Infinity;
    },
    (r) => {
      r.points = Array(30001).fill([0, 0]);
    },
  ]) {
    const f = fixture(),
      before = serialize(f.session.snapshot()),
      raw = structuredClone(f.raw);
    change(raw);
    assert.throws(() => f.send('bake-os-text-outline', { outline: raw }));
    assert.equal(serialize(f.session.snapshot()), before);
    assert.equal(f.session.historyView.past.length, 0);
  }
});

test('locked, driven, styled and unsupported source text refuses rather than discarding semantics', () => {
  for (const change of [
    (n) => {
      n.locked = true;
    },
    (n) => {
      n.params.position.locked = true;
    },
    (n) => {
      n.params.size.expression = '100';
    },
    (n) => {
      n.params.letterSpacing.value = 2;
    },
    (n) => {
      n.text = 'مرحبا';
    },
    (n) => {
      n.text = 'a\nb';
    },
    (n) => {
      n.referenceBounds = 'advance';
    },
    (n) => {
      n.writingMode = 'vertical-rl';
    },
    (n) => {
      n.characterPositions = { styles: { a: { size: 42 } } };
    },
  ]) {
    const f = fixture();
    change(f.text);
    assert.throws(() => osTextIntent(f.text));
  }
  const f = fixture(),
    locked = f.session.snapshot();
  locked.layers[0].locked = true;
  const s = new CommandSession(locked),
    before = serialize(s.snapshot());
  assert.throws(
    () =>
      s.dispatch({
        ...s.state,
        type: 'bake-os-text-outline',
        payload: { layerId: f.layer.id, nodeId: f.text.id, outline: f.raw },
      }),
    /locked/,
  );
  assert.equal(serialize(s.snapshot()), before);
});

test('revision envelopes reject a late native outline and OS conversion can preserve non-default visible alignment', () => {
  const f = fixture(),
    old = f.session.state;
  f.send('rename', { name: 'Changed' });
  const before = serialize(f.session.snapshot());
  assert.throws(
    () =>
      f.session.dispatch({
        ...old,
        type: 'bake-os-text-outline',
        payload: { layerId: f.layer.id, nodeId: f.text.id, outline: f.raw },
      }),
    /STALE|revision/i,
  );
  assert.equal(serialize(f.session.snapshot()), before);
  const g = fixture();
  g.doc.layers[0].nodes.find((n) => n.id === g.text.id).alignment = 'center';
  g.doc.layers[0].nodes.find((n) => n.id === g.text.id).crossAlignment = 'center';
  const s = new CommandSession(g.doc);
  s.dispatch({
    ...s.state,
    type: 'bake-os-text-outline',
    payload: { layerId: g.layer.id, nodeId: g.text.id, outline: g.raw },
  });
  assert.deepEqual(pathBounds(evaluate(s.snapshot().layers[0]).path), [
    [195, 200, 205],
    [295, 300, 305],
  ]);
});
