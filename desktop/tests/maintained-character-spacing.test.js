import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import opentype from 'opentype.js';
import { layoutText } from '../src/text-layout.js';
import {
  document,
  node,
  connect,
  evaluate,
  CommandSession,
  createCharacterPositions,
  setCharacterOffsets,
  setCharacterStyles,
  setCharacterTransforms,
  serialize,
  deserialize,
  validate,
  exportSVG,
  pathData,
} from '../src/core.js';
const load = (p) => {
  const b = fs.readFileSync(new URL(p, import.meta.url));
  return opentype.parse(b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength));
};
const font = load('../public/fonts/DejaVuSans.ttf'),
  jp = load('./fixtures/fonts/NectJPFixture-Regular.otf');
const fonts = { default: font, 'font-jp': jp };
const near = (a, b) => assert.ok(Math.abs(a - b) < 1e-7, `${a} != ${b}`);
function assertGaps(layout, axis = 0, gap = 10, anchor = 100.3125) {
  assert.equal(layout.arrangementStatus.active, true);
  near(layout.characters[0].bounds[axis], anchor);
  layout.characters
    .slice(1)
    .forEach((c, i) => near(c.bounds[axis] - layout.characters[i].bounds[axis + 2], gap));
}
function setup(raw = 'AgW') {
  const doc = document(),
    layer = doc.layers[0],
    t = node('Text'),
    out = node('Output');
  t.text = raw;
  t.referenceBounds = 'advance';
  t.params.position.value = [100, 200];
  t.params.size.value = 40;
  layer.nodes = [t, out];
  layer.output = out.id;
  connect(layer, t.id, out.id, 'path');
  const session = new CommandSession(doc, { fonts: () => fonts });
  const dispatch = (type, p = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: t.id, ...p },
    });
  const current = () => session.snapshot().layers[0].nodes.find((n) => n.id === t.id);
  const layout = () => evaluate(session.snapshot().layers[0], fonts).textLayout(t.id);
  return { doc, layer, t, out, session, dispatch, current, layout };
}
test('exact 40→80→40 ink gap proof, position anchor and no drift', () => {
  const f = setup();
  f.dispatch('set-character-arrangement', { mode: 'equal-ink-gap', gap: 10 });
  assertGaps(f.layout());
  const saved = f.current().characterPositions;
  f.dispatch('set-parameter', { key: 'size', value: 80 });
  assertGaps(f.layout());
  assert.deepEqual(
    f.layout().characters.map((c) => c.bounds.filter((_, i) => i % 2 === 0)),
    [
      [100.3125, 153.75],
      [163.75, 202.8515625],
      [212.8515625, 286.6796875],
    ],
  );
  const p = f.layout().characters.map((c) => c.path);
  for (let i = 0; i < 10; i++)
    assert.deepEqual(
      f.layout().characters.map((c) => c.path),
      p,
    );
  f.dispatch('set-parameter', { key: 'size', value: 40 });
  assertGaps(f.layout());
  assert.deepEqual(f.current().characterPositions, saved);
  f.dispatch('set-parameter', { key: 'position', value: [140, 230] });
  assertGaps(f.layout(), 0, 10, 140.3125);
});
test('authored offsets/styles/rotated ink preserved; disable restores normal outline', () => {
  const f = setup();
  f.t.characterPositions = setCharacterOffsets('AgW', undefined, { c1: [4, 7], c2: [-20, 15] });
  f.t.characterPositions = setCharacterStyles('AgW', f.t.characterPositions, {
    c2: { size: 65, width: 1.3, height: 0.8, baselineShift: 5 },
  });
  f.t.characterPositions = setCharacterTransforms('AgW', f.t.characterPositions, {
    c2: { rotation: 33, scale: 1.2 },
  });
  const record = f.t.characterPositions,
    opts = { position: [100, 200], size: 40, characterPositions: record };
  const ordinary = layoutText(font, 'AgW', opts),
    intent = {
      version: 1,
      mode: 'equal-ink-gap',
      axis: 'x',
      ids: record.units.map((u) => u.id),
      gap: 10,
      anchorOffset: 4.3125,
    };
  const arranged = layoutText(font, 'AgW', { ...opts, characterArrangement: intent });
  assertGaps(arranged, 0, 10, 104.3125);
  arranged.characters.forEach((c, i) => {
    near(c.bounds[1], ordinary.characters[i].bounds[1]);
    near(c.bounds[3], ordinary.characters[i].bounds[3]);
    assert.deepEqual(c.offset, ordinary.characters[i].offset);
  });
  assert.deepEqual(layoutText(font, 'AgW', opts).path.commands, ordinary.path.commands);
});
test('vertical fixture and real alternate font bytes maintain Y gaps after style changes', () => {
  let record = createCharacterPositions('日A');
  record = setCharacterStyles('日A', record, { c2: { font: 'default' } });
  const opts = {
    position: [100, 200],
    size: 40,
    writingMode: 'vertical-rl',
    fontId: 'font-jp',
    fonts,
    characterPositions: record,
  };
  const ordinary = layoutText(jp, '日A', opts),
    anchor = ordinary.characters[0].bounds[1];
  const intent = {
    version: 1,
    mode: 'equal-ink-gap',
    axis: 'y',
    ids: ['c1', 'c2'],
    gap: 10,
    anchorOffset: anchor - 200,
  };
  const first = layoutText(jp, '日A', { ...opts, characterArrangement: intent });
  assertGaps(first, 1, 10, anchor);
  record = setCharacterStyles('日A', record, { c2: { size: 70, width: 1.2, font: 'font-jp' } });
  const second = layoutText(jp, '日A', {
    ...opts,
    characterPositions: record,
    characterArrangement: intent,
  });
  assertGaps(second, 1, 10, anchor);
  const normal = layoutText(jp, '日A', { ...opts, characterPositions: record });
  second.characters.forEach((c, i) => near(c.bounds[0], normal.characters[i].bounds[0]));
});
test('schema24 round trip and strict structural validation; schema23 legacy exact no-op', () => {
  const f = setup();
  const original = f.layout().characters.map((c) => c.path);
  f.doc.schemaVersion = 23;
  assert.deepEqual(
    evaluate(deserialize(serialize(f.doc)).layers[0], fonts)
      .textLayout(f.t.id)
      .characters.map((c) => c.path),
    original,
  );
  f.dispatch('set-character-arrangement', { mode: 'equal-ink-gap', gap: 10 });
  const saved = f.session.snapshot();
  assert.deepEqual(deserialize(serialize(saved)), saved);
  for (const bad of [
    { version: 2 },
    { extra: true },
    { ids: ['c1', 'c1'] },
    { gap: -1 },
    { anchorOffset: Infinity },
  ]) {
    const copy = structuredClone(saved);
    Object.assign(copy.layers[0].nodes[0].characterArrangement, bad);
    assert.throws(() => validate(copy), /arrangement/);
  }
  const copy = structuredClone(saved);
  copy.schemaVersion = 23;
  assert.throws(() => validate(copy), /schemaVersion 24/);
});
test('membership, whitespace, wrap, writing-mode and invalid empty-map basis suspend without deleting intent', () => {
  const record = createCharacterPositions('AgW'),
    intent = {
      version: 1,
      mode: 'equal-ink-gap',
      axis: 'x',
      ids: ['c1', 'c2', 'c3'],
      gap: 10,
      anchorOffset: 0.3125,
    };
  const cases = [
    { text: 'AgW ', characterPositions: createCharacterPositions('AgW ') },
    { text: 'AgW\n', characterPositions: createCharacterPositions('AgW\n') },
    { text: 'AgW', textWrap: { width: 100 } },
    { text: 'AgW', writingMode: 'vertical-rl' },
    { text: 'AgW', characterPositions: createCharacterPositions('ABC') },
    { text: 'AgW', pathAttachment: { nodeId: 'test' } },
    {
      text: 'AgW',
      characterPositions: {
        ...record,
        units: record.units.map((u, i) => ({ ...u, id: `c${i + 4}` })),
        nextId: 7,
      },
    },
  ];
  for (const c of cases) {
    const result = layoutText(font, c.text, {
      position: [100, 200],
      size: 40,
      characterPositions: record,
      characterArrangement: intent,
      ...c,
    });
    assert.equal(result.arrangementStatus.suspended, true, JSON.stringify(c));
    assert.ok(result.arrangementStatus.reason);
    assert.ok(result.characters.every((c) => !c.arrangementOffset));
  }
  assertGaps(
    layoutText(font, 'AgW', {
      position: [100, 200],
      size: 40,
      characterPositions: record,
      characterArrangement: intent,
    }),
  );
});
test('commands are atomic, protected axial edits, cross axis allowed, reset-all clears, undo restores', () => {
  const f = setup();
  const before = f.session.snapshot();
  f.dispatch('set-character-arrangement', { mode: 'equal-ink-gap', gap: 10 });
  const applied = f.session.snapshot();
  for (const [type, p] of [
    ['set-character-offsets', { changes: { c2: [1, 0] } }],
    ['reset-character-adjustments', { mode: 'offsets' }],
    ['set-character-kerning', { changes: { 'c1:c2': 2 } }],
  ])
    assert.throws(() => f.dispatch(type, p), /disable to edit/);
  assert.deepEqual(f.session.snapshot(), applied);
  f.dispatch('set-character-offsets', { changes: { c2: [0, 12] } });
  assertGaps(f.layout());
  f.dispatch('undo');
  assert.deepEqual(f.session.snapshot(), applied);
  f.dispatch('undo');
  assert.deepEqual(f.session.snapshot(), before);
  f.dispatch('redo');
  assert.deepEqual(f.session.snapshot(), applied);
  f.dispatch('reset-character-adjustments', { mode: 'all' });
  assert.equal(f.current().characterArrangement, undefined);
  f.dispatch('undo');
  assertGaps(f.layout());
  f.dispatch('clear-character-arrangement');
  assert.equal(f.current().characterArrangement, undefined);
  f.dispatch('undo');
  assertGaps(f.layout());
});
test('gap scrubs retain anchor and have one Undo; cancellation and no-op add no history', () => {
  const f = setup();
  f.dispatch('set-character-arrangement', { mode: 'equal-ink-gap', gap: 10 });
  const applied = f.session.snapshot(),
    rev = f.session.state.expectedRevision;
  f.dispatch('set-character-arrangement', { updateGap: true, gap: 10 });
  assert.equal(f.session.state.expectedRevision, rev);
  let g = f.dispatch('begin-gesture', {
    target: {
      kind: 'text-values',
      textKind: 'arrangement-gap',
      layerId: f.layer.id,
      nodeId: f.t.id,
    },
  });
  g = f.dispatch('update-gesture', {
    gestureId: g.gestureId,
    expectedPreviewRevision: g.expectedPreviewRevision,
    update: { values: [20] },
  });
  f.dispatch('cancel-gesture', { gestureId: g.gestureId });
  assert.deepEqual(f.session.snapshot(), applied);
  g = f.dispatch('begin-gesture', {
    target: {
      kind: 'text-values',
      textKind: 'arrangement-gap',
      layerId: f.layer.id,
      nodeId: f.t.id,
    },
  });
  g = f.dispatch('update-gesture', {
    gestureId: g.gestureId,
    expectedPreviewRevision: g.expectedPreviewRevision,
    update: { values: [20] },
  });
  f.dispatch('commit-gesture', {
    gestureId: g.gestureId,
    expectedPreviewRevision: g.expectedPreviewRevision,
  });
  assertGaps(f.layout(), 0, 20);
  f.dispatch('undo');
  assert.deepEqual(f.session.snapshot(), applied);
});
test('suspended export and bake refuse; valid bake freezes final geometry and detaches intent', () => {
  const f = setup();
  f.dispatch('set-character-arrangement', { mode: 'equal-ink-gap', gap: 10 });
  assert.match(exportSVG(f.session.snapshot(), fonts), /<svg/);
  f.dispatch('set-text-content', { text: 'Ag W' });
  assert.equal(f.layout().arrangementStatus.suspended, true);
  assert.throws(() => exportSVG(f.session.snapshot(), fonts), /Maintained ink gaps suspended/);
  assert.throws(() => f.dispatch('bake-text-outline'), /Maintained ink gaps suspended/);
  f.dispatch('undo');
  const before = pathData(evaluate(f.session.snapshot().layers[0], fonts).path);
  f.dispatch('bake-text-outline');
  assert.equal(f.current().type, 'Multi Path');
  assert.equal(f.current().characterArrangement, undefined);
  assert.equal(pathData(evaluate(f.session.snapshot().layers[0], fonts).path), before);
});
test('same-layer cache tracks intent and membership replacement never silently enrolls', () => {
  const f = setup();
  f.t.characterPositions = createCharacterPositions(f.t.text);
  const original = evaluate(f.layer, fonts)
    .textLayout(f.t.id)
    .characters.map((c) => c.path);
  f.t.characterArrangement = {
    version: 1,
    mode: 'equal-ink-gap',
    axis: 'x',
    ids: ['c1', 'c2', 'c3'],
    gap: 10,
    anchorOffset: 0.3125,
  };
  assertGaps(evaluate(f.layer, fonts).textLayout(f.t.id));
  f.t.characterArrangement.gap = 17;
  assertGaps(evaluate(f.layer, fonts).textLayout(f.t.id), 0, 17);
  delete f.t.characterArrangement;
  assert.deepEqual(
    evaluate(f.layer, fonts)
      .textLayout(f.t.id)
      .characters.map((c) => c.path),
    original,
  );
  f.dispatch('set-character-arrangement', { mode: 'equal-ink-gap', gap: 10 });
  const applied = f.session.snapshot();
  f.dispatch('set-text-content', {
    text: 'AgW',
    selectionStart: 1,
    selectionEnd: 2,
    inputType: 'insertText',
    forceReplacement: true,
  });
  assert.equal(f.layout().arrangementStatus.suspended, true);
  const retained = f.current().characterArrangement;
  assert.deepEqual(retained, applied.layers[0].nodes[0].characterArrangement);
  f.dispatch('set-character-arrangement', { mode: 'equal-ink-gap', gap: 10 });
  assert.equal(f.layout().arrangementStatus.active, true);
  assert.notDeepEqual(f.current().characterArrangement.ids, retained.ids);
  f.dispatch('undo');
  assert.equal(f.layout().arrangementStatus.suspended, true);
  f.dispatch('undo');
  assert.deepEqual(f.session.snapshot(), applied);
});
test('character drag and numeric gesture cannot bypass axial guard; cross-axis drag works', () => {
  const f = setup();
  f.dispatch('set-character-arrangement', { mode: 'equal-ink-gap', gap: 10 });
  const applied = f.session.snapshot();
  assert.throws(
    () =>
      f.dispatch('begin-gesture', {
        target: {
          kind: 'text-values',
          textKind: 'offset',
          axis: 0,
          ids: ['c2'],
          layerId: f.layer.id,
          nodeId: f.t.id,
        },
      }),
    /disable to edit/,
  );
  let g = f.dispatch('begin-gesture', {
    target: { kind: 'character-translate', ids: ['c2'], layerId: f.layer.id, nodeId: f.t.id },
  });
  assert.throws(
    () => f.dispatch('update-gesture', { ...g, update: { delta: [1, 0] } }),
    /disable to edit/,
  );
  g = f.dispatch('update-gesture', { ...g, update: { delta: [0, 15] } });
  f.dispatch('commit-gesture', g);
  assertGaps(f.layout());
  near(f.current().characterPositions.offsets.c2[1], 15);
  f.dispatch('undo');
  assert.deepEqual(f.session.snapshot(), applied);
});
test('failed Apply/locked target are mutation-free and missing font never substitutes', () => {
  const f = setup('A W'),
    before = f.session.snapshot();
  assert.throws(
    () => f.dispatch('set-character-arrangement', { mode: 'equal-ink-gap', gap: 10 }),
    /whitespace/,
  );
  assert.deepEqual(f.session.snapshot(), before);
  const doc = setup().doc;
  doc.layers[0].nodes[0].locked = true;
  const s = new CommandSession(doc, { fonts: () => fonts });
  assert.throws(
    () =>
      s.dispatch({
        ...s.state,
        type: 'set-character-arrangement',
        payload: {
          layerId: doc.layers[0].id,
          nodeId: doc.layers[0].nodes[0].id,
          mode: 'equal-ink-gap',
          gap: 10,
        },
      }),
    /locked/,
  );
  const record = setCharacterStyles('AgW', undefined, { c2: { font: 'font-missing' } });
  assert.throws(
    () =>
      layoutText(font, 'AgW', {
        position: [100, 200],
        size: 40,
        characterPositions: record,
        fonts,
      }),
    /unavailable/,
  );
});
test('Apply flushes a verified pending draft atomically and Undo restores text and intent', () => {
  const f = setup(),
    before = f.session.snapshot();
  let d = f.dispatch('begin-text-draft');
  d = f.dispatch('stage-text-draft', {
    draftId: d.draftId,
    expectedDraftRevision: d.draftRevision,
    action: 'edit',
    previousRaw: 'AgW',
    nextRaw: 'AgWW',
    selectionStart: 3,
    selectionEnd: 3,
    inputType: 'insertText',
  });
  f.dispatch('set-character-arrangement', { mode: 'equal-ink-gap', gap: 10 });
  assert.equal(f.current().text, 'AgWW');
  assert.equal(f.current().characterArrangement.ids.length, 4);
  assertGaps(f.layout());
  f.dispatch('undo');
  assert.deepEqual(f.session.snapshot(), before);
});
