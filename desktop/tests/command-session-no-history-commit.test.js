import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, node, CommandSession, History } from '../src/core.js';

function fixture(options = {}) {
  const doc = blankDocument();
  const value = node('Value');
  value.id = 'test-value';
  doc.layers[0].nodes.push(value);
  const session = new CommandSession(doc, options);
  const send = (value, envelope = session.state) =>
    session.dispatch({
      ...envelope,
      type: 'set-parameter',
      payload: { layerId: doc.layers[0].id, nodeId: 'test-value', key: 'value', value },
    });
  const read = (doc = session.snapshot()) =>
    doc.layers[0].nodes.find((n) => n.id === 'test-value').params.value.value;
  return { session, send, read };
}

test('project-owned Session replacement preserves frozen old view and detached snapshots', () => {
  const notifications = [];
  const { session, send, read } = fixture({
    retainHistory: false,
    onCommitted: (state) => notifications.push(state),
  });
  const before = session.historyView.doc;
  const oldValue = read(before);
  const revision = session.state.expectedRevision;
  send(5);
  assert.equal(read(), 5);
  assert.equal(read(before), oldValue);
  assert.ok(Object.isFrozen(before));
  assert.equal(session.state.expectedRevision, revision + 1);
  assert.equal(notifications.length, 1);
  assert.equal(session.historyView.past.length, 0);
  assert.equal(session.historyView.future.length, 0);
  const detached = session.snapshot();
  detached.layers[0].nodes.find((n) => n.id === 'test-value').params.value.value = 99;
  assert.equal(read(), 5);
  const stable = session.historyView.doc;
  send(5);
  assert.equal(session.state.expectedRevision, revision + 1);
  assert.equal(session.historyView.doc, stable);
  assert.equal(notifications.length, 1);
});

test('project-owned Session preflight failure leaves document and revision untouched', () => {
  let checks = 0;
  const { session, send, read } = fixture({
    retainHistory: false,
    beforeCandidateCommit: (_before, candidate) => {
      checks++;
      if (candidate.layers[0].nodes.find((n) => n.id === 'test-value').params.value.value === 6)
        throw Error('project preflight rejected');
    },
  });
  send(5);
  const before = session.historyView.doc;
  const state = session.state;
  assert.throws(() => send(6), /project preflight rejected/);
  assert.equal(checks, 2);
  assert.equal(session.historyView.doc, before);
  assert.deepEqual(session.state, state);
  assert.equal(read(), 5);
  assert.throws(() => send(Infinity), /finite|invalid/i);
  assert.equal(session.historyView.doc, before);
  assert.deepEqual(session.state, state);
  send(7);
  assert.equal(read(), 7);
  assert.equal(session.state.expectedRevision, state.expectedRevision + 1);
});

test('retained and project-owned ordinary command results agree through no-op and rejection', () => {
  const kept = fixture();
  const inner = fixture({ retainHistory: false });
  for (const value of [2, 3, 3, -4, Infinity, 8]) {
    const apply = (f) => {
      try {
        return { changed: f.send(value).changed };
      } catch (error) {
        return { error: error.message };
      }
    };
    assert.deepEqual(apply(inner), apply(kept));
    assert.equal(inner.read(), kept.read());
    assert.equal(inner.session.state.expectedRevision, kept.session.state.expectedRevision);
  }
  assert.equal(inner.session.historyView.past.length, 0);
  assert.ok(kept.session.historyView.past.length > 0);
  kept.session.dispatch({ ...kept.session.state, type: 'undo' });
  assert.equal(kept.read(), -4);
  kept.session.dispatch({ ...kept.session.state, type: 'redo' });
  assert.equal(kept.read(), 8);
});

test('generic mutable History.apply still rolls back when retention is disabled', () => {
  const history = new History(blankDocument(), { retainHistory: false });
  const original = JSON.stringify(history.doc);
  assert.throws(
    () =>
      history.apply((doc) => {
        doc.name = 'partial mutation';
        throw Error('abort');
      }),
    /abort/,
  );
  assert.equal(JSON.stringify(history.doc), original);
  assert.equal(history.revision, 0);
  assert.equal(history.past.length, 0);
});

test('preflight normalization back to the old value stays a no-op without notification', () => {
  let checks = 0,
    notifications = 0;
  const { session, send, read } = fixture({
    retainHistory: false,
    onCommitted: () => notifications++,
    beforeCandidateCommit: (before, candidate) => {
      checks++;
      const get = (doc) => doc.layers[0].nodes.find((n) => n.id === 'test-value').params.value;
      get(candidate).value = get(before).value;
    },
  });
  const original = read(),
    state = session.state;
  assert.equal(send(7).changed, false);
  assert.equal(checks, 1);
  assert.equal(read(), original);
  assert.deepEqual(session.state, state);
  assert.equal(notifications, 0);
});

test('stale no-history command cannot reach candidate preflight or change the newer value', () => {
  let checks = 0;
  const { session, send, read } = fixture({
    retainHistory: false,
    beforeCandidateCommit: () => checks++,
  });
  const stale = session.state;
  send(3);
  const latest = session.historyView.doc,
    state = session.state;
  assert.throws(() => send(4, stale), /STALE_REVISION/);
  assert.equal(checks, 1);
  assert.equal(read(), 3);
  assert.equal(session.historyView.doc, latest);
  assert.deepEqual(session.state, state);
});
