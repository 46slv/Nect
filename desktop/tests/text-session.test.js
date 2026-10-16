import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { CommandSession, document, node, serialize, setCharacterOffsets } from '../src/core.js';

function fixture() {
  const doc = document(),
    layer = doc.layers[0],
    text = node('Text');
  text.text = 'ABA';
  doc.schemaVersion = 3;
  text.characterPositions = setCharacterOffsets(text.text, undefined, { c2: [12, -7] });
  layer.nodes.push(text);
  const session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: layer.id, nodeId: text.id, ...payload },
      });
  let draft;
  const begin = () => (draft = send('begin-text-draft'));
  const stage = (payload) =>
    (draft = send('stage-text-draft', {
      draftId: draft.draftId,
      expectedDraftRevision: draft.draftRevision,
      ...payload,
    }));
  const commit = () =>
    send('commit-text-draft', {
      draftId: draft.draftId,
      expectedDraftRevision: draft.draftRevision,
    });
  const current = () => session.snapshot().layers[0].nodes.find((n) => n.id === text.id);
  return { session, send, text, begin, stage, commit, current };
}

test('character commands preserve mixed transforms and maps, reject partial IDs and share reset/Undo/Redo', () => {
  const f = fixture(),
    before = f.session.snapshot();
  f.send('set-character-transforms', { characterIds: ['c1'], patch: { rotation: 35 } });
  f.send('set-character-transforms', { characterIds: ['c2'], patch: { rotation: -12, scale: 2 } });
  f.send('set-character-transforms', { characterIds: ['c1', 'c2'], patch: { scale: 3 } });
  assert.deepEqual(f.current().characterPositions.transforms, {
    c1: { rotation: 35, scale: 3 },
    c2: { rotation: -12, scale: 3 },
  });
  const moved = f.current().characterPositions.offsets.c2;
  assert.deepEqual(moved, [12, -7]);
  f.send('reset-character-adjustments', { mode: 'transforms', characterIds: ['c1'] });
  const preserved = f.session.snapshot(),
    state = f.session.state;
  for (const [type, payload] of [
    ['set-character-offsets', { changes: { c1: [4, 5], foreign: [6, 7] } }],
    ['set-character-transforms', { characterIds: ['c1', 'foreign'], patch: { scale: 4 } }],
    ['set-character-transforms', { characterIds: ['c1'], patch: { scale: 0 } }],
    ['set-character-transforms', { characterIds: ['c1'], patch: { text: 'injected' } }],
    ['reset-character-adjustments', { mode: 'foreign' }],
  ]) {
    assert.throws(() => f.send(type, payload));
    assert.deepEqual(f.session.snapshot(), preserved);
    assert.deepEqual(f.session.state, state);
  }
  f.send('reset-character-adjustments', { mode: 'all' });
  const reset = f.session.snapshot();
  f.send('undo');
  assert.deepEqual(f.session.snapshot(), preserved);
  const revision = f.session.state.expectedRevision;
  f.send('set-character-offsets', { changes: { c2: moved } });
  assert.equal(f.session.state.expectedRevision, revision);
  f.send('redo');
  assert.deepEqual(f.session.snapshot(), reset);
  assert.equal(f.current().text, before.layers[0].nodes.at(-1).text);
});

test('pair commands validate layout and stable ordered pairs before committing', () => {
  const f = fixture(),
    data = fs.readFileSync(new URL('./fixtures/fonts/NectJPFixture-Regular.otf', import.meta.url));
  f.send('adopt-font', {
    bytes: data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength),
    fileName: 'Fixture',
  });
  f.send('set-character-kerning', { changes: { 'c1:c2': -55 } });
  assert.equal(f.current().characterPositions.kerningPairs['c1:c2'], -55);
  const before = f.session.snapshot(),
    state = f.session.state;
  assert.throws(() => f.send('set-character-kerning', { changes: { 'c1:c2': 20, 'c1:c3': 25 } }));
  assert.deepEqual(f.session.snapshot(), before);
  assert.deepEqual(f.session.state, state);
  f.send('set-character-kerning', { changes: { 'c1:c2': null } });
  assert.deepEqual(f.current().characterPositions.kerningPairs, {});
  f.send('undo');
  assert.deepEqual(f.session.snapshot(), before);
});

test('Session draft owns repeated-character replacements and native Undo/Redo; commit is one document edit', () => {
  const f = fixture(),
    before = serialize(f.session.snapshot()),
    state = f.session.state;
  f.begin();
  let draft = f.stage({
    action: 'edit',
    previousRaw: 'ABA',
    nextRaw: 'ABA',
    selectionStart: 1,
    selectionEnd: 2,
    inputType: 'insertFromPaste',
  });
  assert.equal(draft.changed, true);
  assert.equal(serialize(f.session.snapshot()), before, 'draft must not mutate authored state');
  assert.deepEqual(f.session.state, state);
  assert.equal('record' in draft, false, 'adapter receives no authored map');
  draft = f.stage({ action: 'edit', previousRaw: 'ABA', nextRaw: 'ABA', inputType: 'historyUndo' });
  assert.equal(draft.changed, false);
  f.stage({ action: 'edit', previousRaw: 'ABA', nextRaw: 'ABA', inputType: 'historyRedo' });
  f.commit();
  assert.notEqual(
    f.current().characterPositions.units[1].id,
    f.text.characterPositions.units[1].id,
  );
  assert.deepEqual(f.current().characterPositions.offsets, {});
  assert.equal(f.session.legacyHistory.past.length, 1);
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), before);
  f.send('redo');
  assert.deepEqual(f.current().characterPositions.offsets, {});
});

test('composition captures its initial range, stages once and blocks authored commands until completed/cancelled', () => {
  const f = fixture();
  f.begin();
  const start = f.stage({
    action: 'composition-start',
    previousRaw: 'ABA',
    selectionStart: 1,
    selectionEnd: 2,
  });
  assert.throws(() => f.send('undo'), /GESTURE_ACTIVE/);
  assert.throws(() => f.commit(), /GESTURE_ACTIVE/);
  assert.throws(
    () => f.stage({ action: 'edit', previousRaw: 'ABA', nextRaw: 'A仮A' }),
    /composition/,
  );
  const final = f.stage({ action: 'composition-end', nextRaw: 'A文A' });
  assert.equal(final.draftRevision, start.draftRevision + 1);
  assert.equal(final.composing, false);
  f.commit();
  assert.equal(f.current().text, 'A文A');
  assert.equal(f.current().characterPositions.units[0].id, f.text.characterPositions.units[0].id);
  assert.equal(f.current().characterPositions.units[2].id, f.text.characterPositions.units[2].id);
  f.begin();
  f.stage({ action: 'composition-start', previousRaw: 'A文A', selectionStart: 1, selectionEnd: 2 });
  const cancelled = f.stage({ action: 'composition-end', nextRaw: 'A文A' });
  assert.equal(cancelled.changed, false);
  const revision = f.session.state.expectedRevision;
  f.commit();
  assert.equal(f.session.state.expectedRevision, revision);
});

test('malformed/stale/foreign drafts cannot alter authored state or erase native identity evidence', () => {
  const f = fixture(),
    initial = f.begin(),
    before = serialize(f.session.snapshot());
  assert.throws(() => f.stage({ action: 'edit', previousRaw: 'wrong', nextRaw: 'AB' }), /STALE/);
  assert.throws(() => f.stage({ action: 'edit', previousRaw: 'ABA', nextRaw: '\ud800' }));
  assert.throws(
    () =>
      f.send('stage-text-draft', {
        draftId: initial.draftId,
        expectedDraftRevision: -1,
        action: 'edit',
        previousRaw: 'ABA',
        nextRaw: 'AB',
      }),
    /STALE/,
  );
  assert.equal(serialize(f.session.snapshot()), before);
  assert.throws(
    () =>
      f.stage({
        action: 'edit',
        previousRaw: 'ABA',
        nextRaw: 'ABA',
        inputType: 'historyUndo',
        forceReplacement: 'yes',
      }),
    /Invalid/,
  );
  f.stage({ action: 'edit', previousRaw: 'ABA', nextRaw: 'ABBA', inputType: 'historyUndo' });
  f.commit();
  assert.equal(
    f.current().characterPositions.basis,
    'ABA',
    'unknown native history suspends adjusted identities',
  );
  assert.equal(f.current().text, 'ABBA');
  const stale = f.begin();
  f.send('undo');
  f.send('redo');
  assert.throws(
    () =>
      f.send('commit-text-draft', {
        draftId: stale.draftId,
        expectedDraftRevision: stale.draftRevision,
      }),
    /STALE/,
  );
  const foreign = fixture();
  assert.throws(
    () =>
      foreign.send('commit-text-draft', {
        draftId: stale.draftId,
        expectedDraftRevision: stale.draftRevision,
      }),
    /STALE/,
  );
});

test('direct intent and draft commit agree; no-op commit preserves redo and legacy map changes refuse', () => {
  const f = fixture(),
    copy = new CommandSession(f.session.snapshot());
  const intent = { selectionStart: 1, selectionEnd: 2, inputType: 'insertText' };
  f.begin();
  f.stage({ action: 'edit', previousRaw: 'ABA', nextRaw: 'AXA', ...intent });
  f.commit();
  copy.dispatch({
    ...copy.state,
    type: 'set-text-content',
    payload: {
      layerId: f.session.snapshot().layers[0].id,
      nodeId: f.text.id,
      text: 'AXA',
      ...intent,
    },
  });
  assert.deepEqual(copy.snapshot(), f.session.snapshot());
  f.send('undo');
  const revision = f.session.state.expectedRevision,
    redo = f.session.legacyHistory.future.length;
  f.begin();
  f.commit();
  assert.equal(f.session.state.expectedRevision, revision);
  assert.equal(f.session.legacyHistory.future.length, redo);
  f.begin();
  f.session.legacyHistory.doc.layers[0].nodes.find(
    (n) => n.id === f.text.id,
  ).characterPositions.offsets.c2 = [1, 2];
  assert.throws(() => f.commit(), /STALE_TEXT_DRAFT/);
});

test('font adoption owns preparation, assets and references atomically; stale bytes cannot follow Undo back to an equal document', () => {
  const f = fixture(),
    before = serialize(f.session.snapshot());
  const bytes = (name) => {
    const data = fs.readFileSync(new URL(`./fixtures/fonts/${name}`, import.meta.url));
    return data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
  };
  const raw = bytes('NectJPFixture-Regular.otf');
  const adopted = f.send('adopt-font', { bytes: raw, fileName: 'Japanese fixture' });
  assert.equal(f.current().font, adopted.fontId);
  assert.deepEqual(Object.keys(f.session.snapshot().fontAssets), [adopted.fontId]);
  const revision = f.session.state.expectedRevision;
  f.send('adopt-font', { bytes: raw, fileName: 'same bytes' });
  assert.equal(f.session.state.expectedRevision, revision, 'deduplication is a no-op');
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), before);
  f.send('redo');
  const state = f.session.state,
    saved = serialize(f.session.snapshot());
  f.send('set-parameter', { nodeId: f.text.id, key: 'size', value: 31 });
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), saved);
  assert.throws(
    () =>
      f.session.dispatch({
        ...state,
        type: 'adopt-font',
        payload: {
          layerId: f.session.snapshot().layers[0].id,
          nodeId: f.text.id,
          bytes: raw,
          fileName: 'late response',
        },
      }),
    /STALE_REVISION/,
  );
  const range = f.send('adopt-font', {
    bytes: bytes('NectJPFixture-Regular.ttf'),
    fileName: 'Range font',
    characterIds: ['c2'],
  });
  assert.equal(f.current().font, adopted.fontId);
  assert.equal(f.current().characterPositions.styles.c2.font, range.fontId);
  assert.equal(Object.keys(f.session.snapshot().fontAssets).length, 2);
  const preserved = serialize(f.session.snapshot()),
    envelope = f.session.state;
  for (const invalid of [new ArrayBuffer(2), new ArrayBuffer(20_000_001)]) {
    assert.throws(() => f.send('adopt-font', { bytes: invalid, fileName: 'bad' }));
    assert.equal(serialize(f.session.snapshot()), preserved);
    assert.deepEqual(f.session.state, envelope);
  }
  f.send('set-text-content', { text: '😀' });
  const unsupported = serialize(f.session.snapshot());
  assert.throws(() => f.send('adopt-font', { bytes: raw, fileName: 'missing glyph' }));
  assert.equal(serialize(f.session.snapshot()), unsupported);
});

test('Text settings preserve vertical text; invalid wrap/typography/map injection cannot partially commit', () => {
  const f = fixture();
  f.send('set-text-settings', {
    settings: {
      writingMode: 'vertical-rl',
      alignment: 'center',
      leading: { mode: 'manual', value: 42 },
    },
  });
  assert.equal(f.current().writingMode, 'vertical-rl');
  const preserved = serialize(f.session.snapshot()),
    state = f.session.state;
  for (const settings of [
    { alignment: 'end', textWrap: { width: -1 } },
    { alignment: 'bad' },
    { characterPositions: {} },
  ]) {
    assert.throws(() => f.send('set-text-settings', { settings }));
    assert.equal(serialize(f.session.snapshot()), preserved);
    assert.deepEqual(f.session.state, state);
  }
  assert.throws(() => f.send('set-character-style', { characterIds: ['c2'], patch: { size: -1 } }));
  assert.equal(serialize(f.session.snapshot()), preserved);
  f.send('undo');
  assert.equal(f.current().writingMode, f.text.writingMode);
});
