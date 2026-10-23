import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, document, node, serialize, deserialize } from '../src/core.js';
import { createCharacterPositions, setCharacterOffsets } from '../src/character-identity.js';
import { graphemeSamples } from './fixtures/grapheme-editing.js';
import { graphemeDeletionRange } from '../src/grapheme-editing.js';

function fixture(sample) {
  const doc = document(),
    layer = doc.layers[0],
    text = node('Text');
  text.text = `A${sample.text}B`;
  const record = createCharacterPositions(text.text);
  text.characterPositions = setCharacterOffsets(text.text, record, {
    [record.units.at(-1).id]: [12, -7],
  });
  layer.nodes.push(text);
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
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
  return {
    session,
    send,
    begin,
    stage,
    commit,
    raw: text.text,
    original: serialize(session.snapshot()),
  };
}

for (const sample of graphemeSamples) {
  test(`whole-grapheme deletion preserves native draft history, Session Undo/Redo and encoding: ${sample.name}`, () => {
    const f = fixture(sample);
    f.begin();
    const range = graphemeDeletionRange(f.raw, {
      start: 1 + sample.text.length,
      end: 1 + sample.text.length,
      inputType: 'deleteContentBackward',
    });
    const deletion = {
      action: 'edit',
      previousRaw: f.raw,
      nextRaw: 'AB',
      selectionStart: range.start,
      selectionEnd: range.end,
      inputType: 'deleteContentBackward',
    };
    f.stage(deletion);
    const removed = f.session.textDraftSnapshot(
      f.stage({ action: 'edit', previousRaw: 'AB', nextRaw: f.raw, inputType: 'historyUndo' })
        .draftId,
    ).document;
    assert.equal(serialize(removed), f.original, 'native Undo restores character IDs and offsets');
    f.stage({ action: 'edit', previousRaw: f.raw, nextRaw: 'AB', inputType: 'historyRedo' });
    assert.equal(
      serialize(f.session.snapshot()),
      f.original,
      'draft stays outside authored/save state',
    );
    f.commit();
    const edited = serialize(f.session.snapshot());
    assert.equal(f.session.snapshot().layers[0].nodes.at(-1).text, 'AB');
    assert.equal(
      serialize(deserialize(edited)),
      edited,
      'saved JSON retains deletion and remaining IDs',
    );
    f.send('undo');
    assert.equal(serialize(f.session.snapshot()), f.original);
    f.send('redo');
    assert.equal(serialize(f.session.snapshot()), edited);
  });

  test(`composition over a grapheme remains uncommitted until completion: ${sample.name}`, () => {
    const f = fixture(sample);
    f.begin();
    f.stage({
      action: 'composition-start',
      previousRaw: f.raw,
      selectionStart: 1,
      selectionEnd: 1 + sample.text.length,
    });
    assert.equal(serialize(f.session.snapshot()), f.original);
    assert.throws(() => f.commit(), /GESTURE_ACTIVE/);
    assert.throws(() => f.send('undo'), /GESTURE_ACTIVE/);
    assert.equal(serialize(f.session.snapshot()), f.original);
    f.stage({ action: 'composition-end', nextRaw: 'A\u304bB' });
    assert.equal(serialize(f.session.snapshot()), f.original);
    f.commit();
    const saved = serialize(f.session.snapshot());
    assert.equal(f.session.snapshot().layers[0].nodes.at(-1).text, 'A\u304bB');
    assert.equal(serialize(deserialize(saved)), saved);
    f.send('undo');
    assert.equal(serialize(f.session.snapshot()), f.original);
    f.send('redo');
    assert.equal(serialize(f.session.snapshot()), saved);
  });
}
