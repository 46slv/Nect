import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, document, node, point, serialize, evaluate } from '../src/core.js';

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

test('parameter commands share expressions, fallback, locks and Undo without mutating caller values', () => {
  const { session: s, dispatch, a, b } = fixture();
  dispatch('set-parameter', { nodeId: a.id, key: 'a', value: 12 });
  dispatch('set-parameter-expression', { nodeId: a.id, key: 'a', expression: '3 * 7' });
  assert.equal(evaluate(s.snapshot().layers[0]).run(a.id), 21);
  dispatch('connect', { sourceId: a.id, nodeId: b.id, key: 'a' });
  assert.equal(evaluate(s.snapshot().layers[0]).run(b.id), 21);
  const connected = serialize(s.snapshot()),
    state = s.state;
  assert.throws(() => dispatch('set-parameter', { nodeId: b.id, key: 'a', value: 99 }), /driven/);
  assert.throws(() => dispatch('set-parameter', { nodeId: a.id, key: 'a', value: 99 }), /driven/);
  assert.equal(serialize(s.snapshot()), connected);
  assert.deepEqual(s.state, state);
  dispatch('disconnect', { nodeId: b.id, key: 'a' });
  assert.equal(evaluate(s.snapshot().layers[0]).run(b.id), 17);
  dispatch('set-parameter-lock', { nodeId: a.id, key: 'a', locked: true });
  assert.throws(
    () => dispatch('set-parameter-expression', { nodeId: a.id, key: 'a', expression: '' }),
    /locked/,
  );
  dispatch('set-parameter-lock', { nodeId: a.id, key: 'a', locked: false });
  dispatch('set-parameter-expression', { nodeId: a.id, key: 'a', expression: '' });
  assert.equal(evaluate(s.snapshot().layers[0]).run(a.id), 12);
  const before = serialize(s.snapshot());
  dispatch('set-parameter', { nodeId: a.id, key: 'a', value: 28 });
  const after = serialize(s.snapshot());
  dispatch('undo');
  assert.equal(serialize(s.snapshot()), before);
  dispatch('redo');
  assert.equal(serialize(s.snapshot()), after);
});

test('invalid properties preserve authored state, revision and redo; point keys use stable IDs', () => {
  const doc = document(),
    layer = doc.layers[0],
    curve = node('Curve'),
    color = node('Color');
  curve.points = [point(1, 2), point(3, 4)];
  layer.nodes.push(curve, color);
  const s = new CommandSession(doc),
    dispatch = (type, payload) =>
      s.dispatch({ ...s.state, type, payload: { layerId: layer.id, ...payload } });
  dispatch('set-parameter', {
    nodeId: curve.id,
    key: `${curve.points[1].id}.position`,
    axis: 0,
    value: 43,
  });
  assert.deepEqual(
    s
      .snapshot()
      .layers[0].nodes.find((n) => n.id === curve.id)
      .points.map((p) => p.position.value),
    [
      [1, 2],
      [43, 4],
    ],
  );
  const paint = { hex: '#123456', alpha: 0.4 };
  dispatch('set-parameter', { nodeId: color.id, key: 'value', value: paint });
  paint.alpha = 0.9;
  assert.equal(
    s.snapshot().layers[0].nodes.find((n) => n.id === color.id).params.value.value.alpha,
    0.4,
  );
  dispatch('undo');
  const before = serialize(s.snapshot()),
    state = s.state,
    redo = s.legacyHistory.future.length;
  for (const [type, payload] of [
    ['set-parameter', { nodeId: color.id, key: 'value', value: { hex: 'bad', alpha: 2 } }],
    [
      'set-parameter',
      { nodeId: curve.id, key: `${curve.points[1].id}.position`, axis: 2, value: 4 },
    ],
    [
      'set-parameter',
      { nodeId: curve.id, key: `${curve.points[1].id}.position`, axis: 0, value: NaN },
    ],
    ['set-parameter', { nodeId: curve.id, key: 'unknown', value: 4 }],
    [
      'set-parameter-expression',
      { nodeId: curve.id, key: `${curve.points[1].id}.position`, expression: '1' },
    ],
    ['set-parameter-expression', { nodeId: color.id, key: 'value', expression: '1' }],
    ['set-parameter-lock', { nodeId: color.id, key: 'value', locked: 'true' }],
  ]) {
    assert.throws(() => dispatch(type, payload));
    assert.equal(serialize(s.snapshot()), before);
    assert.deepEqual(s.state, state);
    assert.equal(s.legacyHistory.future.length, redo);
  }
  dispatch('redo');
  assert.equal(
    s.snapshot().layers[0].nodes.find((n) => n.id === color.id).params.value.value.alpha,
    0.4,
  );
});
