import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, document, node, serialize } from '../src/core.js';

function fixture(options) {
  const doc = document(),
    layer = doc.layers[0],
    a = node('Math'),
    b = node('Math');
  b.params.a.value = 17;
  layer.nodes.push(a, b);
  const session = new CommandSession(doc, options);
  const dispatch = (type, payload = {}) =>
    session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } });
  return { session, dispatch, a, b, layer };
}

test('typed graph commands preserve fallback and stable IDs through shared Undo/Redo', () => {
  const { session: s, dispatch, a, b } = fixture();
  const before = serialize(s.snapshot());
  dispatch('connect', { sourceId: a.id, nodeId: b.id, key: 'a' });
  const connected = serialize(s.snapshot()),
    request = s.state;
  assert.equal(s.snapshot().layers[0].nodes.find((n) => n.id === b.id).params.a.value, 17);
  dispatch('undo');
  assert.equal(serialize(s.snapshot()), before);
  assert.ok(s.state.expectedRevision > request.expectedRevision);
  dispatch('redo');
  assert.equal(serialize(s.snapshot()), connected);
  dispatch('disconnect', { nodeId: b.id, key: 'a' });
  assert.equal(serialize(s.snapshot()), before);
  const revision = s.state.expectedRevision;
  dispatch('disconnect', { nodeId: b.id, key: 'a' });
  assert.equal(s.state.expectedRevision, revision, 'no-op must not record history');
  dispatch('set-operation', { nodeId: b.id, operation: 'multiply' });
  assert.equal(s.snapshot().layers[0].nodes.find((n) => n.id === b.id).operation, 'multiply');
});

test('foreign/stale envelopes, cycles, invalid operations and locks refuse atomically', () => {
  const { session: s, dispatch, a, b } = fixture();
  const stale = s.state;
  dispatch('connect', { sourceId: a.id, nodeId: b.id, key: 'a' });
  const before = serialize(s.snapshot()),
    state = s.state;
  for (const command of [
    { ...stale, type: 'undo' },
    { ...state, sessionId: 'foreign', type: 'undo' },
    {
      ...state,
      type: 'connect',
      payload: { layerId: s.snapshot().layers[0].id, sourceId: b.id, nodeId: a.id, key: 'a' },
    },
    {
      ...state,
      type: 'set-operation',
      payload: { layerId: s.snapshot().layers[0].id, nodeId: a.id, operation: 'unsupported' },
    },
  ]) {
    assert.throws(() => s.dispatch(command));
    assert.equal(serialize(s.snapshot()), before);
    assert.deepEqual(s.state, state);
  }
  const locked = s.snapshot();
  locked.layers[0].locked = true;
  const other = new CommandSession(locked);
  assert.throws(
    () =>
      other.dispatch({
        ...other.state,
        type: 'disconnect',
        payload: { layerId: locked.layers[0].id, nodeId: b.id, key: 'a' },
      }),
    /locked/,
  );
});

test('snapshot is detached; active gestures block commands; transitional UI shares one History', () => {
  let busy = true;
  const { session: s, dispatch, a } = fixture({ busy: () => busy });
  assert.throws(() => dispatch('undo'), /GESTURE_ACTIVE/);
  busy = false;
  const copy = s.snapshot();
  copy.name = 'detached';
  assert.notEqual(s.snapshot().name, 'detached');
  const request = s.state;
  s.legacyHistory.apply((doc) => {
    doc.name = 'legacy edit';
  });
  assert.throws(() => s.dispatch({ ...request, type: 'undo' }), /STALE_REVISION/);
  dispatch('undo');
  assert.notEqual(s.snapshot().name, 'legacy edit');
  dispatch('set-operation', { nodeId: a.id, operation: 'multiply' });
  assert.equal(s.legacyHistory.past.length, 1, 'one command, one shared Undo entry');
});
