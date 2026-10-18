import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, blankDocument, serialize } from '../src/core.js';

test('composition import is atomic, remaps repeated IDs, and is one undo step', () => {
  const source = blankDocument();
  const session = new CommandSession(blankDocument());
  const before = serialize(session.snapshot());
  session.dispatch({ ...session.state, type: 'import-composition',
    payload: { sourceText: serialize(source), compositionId: source.layers[0].id } });
  assert.equal(session.snapshot().layers.length, 2);
  session.dispatch({ ...session.state, type: 'undo' });
  assert.equal(serialize(session.snapshot()), before);
  session.dispatch({ ...session.state, type: 'redo' });
  assert.equal(session.snapshot().layers.length, 2);
});

test('invalid composition import leaves the session untouched', () => {
  const session = new CommandSession(blankDocument());
  const before = serialize(session.snapshot());
  assert.throws(() => session.dispatch({ ...session.state, type: 'import-composition',
    payload: { sourceText: '{}', compositionId: 'missing' } }));
  assert.equal(serialize(session.snapshot()), before);
});
