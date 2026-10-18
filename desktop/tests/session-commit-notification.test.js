import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, document, node, serialize } from '../src/core.js';
test('committed notification covers Inspector edits, paste/delete, Undo/Redo but not reads, failures or no-ops', () => {
  const d = document(),
    l = d.layers[0],
    circle = l.nodes[0],
    events = [];
  const s = new CommandSession(d, {
    onCommitted: (state) => events.push({ state, doc: serialize(s.snapshot()) }),
  });
  const send = (type, payload = {}) =>
    s.dispatch({ ...s.state, type, payload: { layerId: l.id, ...payload } });
  send('set-parameter', { nodeId: circle.id, key: 'radius', value: 55 });
  assert.equal(events.length, 1);
  send('set-parameter', { nodeId: circle.id, key: 'radius', value: 55 });
  assert.equal(events.length, 1);
  assert.throws(() => send('set-parameter', { nodeId: 'missing', key: 'radius', value: 40 }));
  assert.equal(events.length, 1);
  send('copy-nodes', { nodeIds: [circle.id] });
  assert.equal(events.length, 1);
  const pasted = send('paste-nodes');
  assert.equal(events.length, 2);
  send('delete-nodes', { nodeIds: pasted.nodeIds });
  assert.equal(events.length, 3);
  send('undo');
  send('redo');
  assert.equal(events.length, 5);
  assert.deepEqual(
    events.map((e) => e.state.expectedRevision),
    [1, 2, 3, 4, 5],
  );
});
test('gesture previews and IME drafts notify only when committed; observer runs after cleanup', () => {
  const d = document(),
    l = d.layers[0],
    circle = l.nodes[0],
    text = node('Text');
  text.text = 'A';
  l.nodes.push(text);
  const events = [];
  const s = new CommandSession(d, {
    onCommitted: (state) =>
      events.push({
        revision: state.expectedRevision,
        gesture: s.hasGesture,
        composition: s.hasTextComposition,
      }),
  });
  const send = (type, payload = {}) =>
    s.dispatch({ ...s.state, type, payload: { layerId: l.id, ...payload } });
  let g = send('begin-gesture', {
    target: { kind: 'nodes-translate', layerId: l.id, nodeIds: [circle.id] },
  });
  g = send('update-gesture', { ...g, update: { delta: [20, 10] } });
  assert.equal(events.length, 0);
  send('commit-gesture', g);
  assert.deepEqual(events, [{ revision: 1, gesture: false, composition: false }]);
  let t = send('begin-text-draft', { nodeId: text.id });
  const stage = (payload) =>
    (t = send('stage-text-draft', {
      draftId: t.draftId,
      expectedDraftRevision: t.draftRevision,
      ...payload,
    }));
  stage({ action: 'composition-start', previousRaw: 'A', selectionStart: 1, selectionEnd: 1 });
  assert.equal(events.length, 1);
  stage({ action: 'composition-end', nextRaw: 'AB' });
  assert.equal(events.length, 1);
  send('commit-text-draft', { draftId: t.draftId, expectedDraftRevision: t.draftRevision });
  assert.equal(events.length, 2);
  assert.equal(events[1].composition, false);
});
test('prepared history commits notify once after activation and an observer error cannot reject the edit', async () => {
  const d = document(),
    l = d.layers[0],
    circle = l.nodes[0],
    events = [];
  let activated = false;
  const s = new CommandSession(d, {
    onCommitted: (state) => {
      events.push({ revision: state.expectedRevision, activated });
      throw Error('Adapter failure');
    },
  });
  s.dispatch({
    ...s.state,
    type: 'set-parameter',
    payload: { layerId: l.id, nodeId: circle.id, key: 'radius', value: 55 },
  });
  const bridge = {
    prepareDocument: async () => ({}),
    preparedFonts: () => ({}),
    assertFresh: () => {},
    activate: () => {
      activated = true;
    },
  };
  const token = await s.prepareCommand({ ...s.state, type: 'undo' }, bridge);
  assert.equal(events.length, 1);
  s.preparedSnapshot(token);
  assert.equal(events.length, 1);
  s.commitPrepared(token);
  assert.deepEqual(events.at(-1), { revision: 2, activated: true });
  assert.equal(s.state.expectedRevision, 2);
});
