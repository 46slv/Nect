import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  evaluate,
  CommandSession,
  expression,
  expressionReferences,
  serialize,
  deserialize,
  duplicateNodes,
} from '../src/core.js';
function fixture(type = 'Value') {
  const doc = document(),
    l = doc.layers[0],
    a = node(type),
    b = node(type);
  l.nodes.push(a, b);
  a.params.value.value = type === 'Vector' ? [100, 200] : 100;
  b.params.value.value = type === 'Vector' ? [20, 10] : 20;
  const s = new CommandSession(doc);
  const send = (type, payload = {}) =>
    s.dispatch({
      ...s.state,
      type,
      payload: { layerId: l.id, nodeId: b.id, key: 'value', ...payload },
    });
  const q = () => s.snapshot().layers[0].nodes.find((n) => n.id === b.id).params.value;
  const result = () => evaluate(s.snapshot().layers[0]).run(b.id);
  return { s, send, q, result, a, b };
}
test('scalar authored base is editable without accumulation; undo/redo and persistence preserve syntax and base', () => {
  const { s, send, q, result } = fixture();
  send('set-parameter-expression', { expression: 'value + 1' });
  assert.equal(s.snapshot().schemaVersion, 26);
  for (let i = 0; i < 100; i++) assert.equal(result(), 21);
  assert.equal(q().value, 20);
  send('set-parameter-base', { value: 30 });
  assert.equal(result(), 31);
  assert.equal(q().expression, 'value + 1');
  send('undo');
  assert.equal(result(), 21);
  send('redo');
  assert.equal(result(), 31);
  assert.equal(deserialize(serialize(s.snapshot())).schemaVersion, 26);
  assert.throws(() => send('set-parameter', { value: 50 }), /driven/);
});
test('XY value is component-local, reference is effective, duplication retains independent base', () => {
  const { s, send, q, result, a, b } = fixture('Vector');
  const expr = `ref("${a.id}", "value", "x") + value; ref("${a.id}", "value", "y") + value`;
  send('set-parameter-expression', { expression: expr });
  assert.deepEqual(result(), [120, 210]);
  const count = s.snapshot().layers[0].nodes.length;
  send('set-parameter-base', { axis: 0, value: 30 });
  assert.deepEqual(result(), [130, 210]);
  assert.deepEqual(q().value, [30, 10]);
  assert.equal(s.snapshot().layers[0].nodes.length, count);
  const l = deserialize(serialize(s.snapshot())).layers[0];
  const [ca, cb] = duplicateNodes(l, [a.id, b.id]);
  ca.params.value.value = [400, 500];
  cb.params.value.value = [1, 2];
  assert.deepEqual(evaluate(l).run(cb.id), [401, 502]);
  assert.deepEqual(evaluate(l).run(b.id), [130, 210]);
  assert.deepEqual(
    expressionReferences(expr).map((r) => r.nodeId),
    [a.id, a.id],
  );
});
test('base edits are atomic for nonfinite results, wrong types, stale revisions and locks', () => {
  const { s, send, q } = fixture();
  send('set-parameter-expression', { expression: '1 / value' });
  const before = serialize(s.snapshot()),
    revision = s.state.expectedRevision;
  for (const value of [0, NaN, Infinity, [2, 3], true]) {
    assert.throws(() => send('set-parameter-base', { value }));
    assert.equal(serialize(s.snapshot()), before);
    assert.equal(s.state.expectedRevision, revision);
  }
  const stale = s.state;
  send('set-parameter-base', { value: 3 });
  assert.throws(
    () => s.dispatch({ ...stale, type: 'set-parameter-base', payload: { value: 4 } }),
    /STALE/,
  );
  send('set-parameter-lock', { locked: true });
  assert.throws(() => send('set-parameter-base', { value: 4 }), /locked/);
  assert.equal(q().value, 3);
});
test('source wins; dormant base edits retain source/expression and resume on disconnect', () => {
  const { send, q, result, a } = fixture();
  send('set-parameter-expression', { expression: 'value + 1' });
  send('connect', { sourceId: a.id });
  assert.equal(result(), 100);
  send('set-parameter-base', { value: 40 });
  assert.equal(result(), 100);
  assert.equal(q().source, a.id);
  assert.equal(q().expression, 'value + 1');
  send('disconnect');
  assert.equal(result(), 41);
});
test('self effective cycles and unsupported whole vectors still reject; bare value requires explicit version', () => {
  const { s, send, b } = fixture();
  assert.throws(
    () => send('set-parameter-expression', { expression: `ref("${b.id}", "value") + value` }),
    /cycle/i,
  );
  assert.throws(() => expression('value + 1'));
  assert.equal(expression('value + 1', undefined, false, 5), 6);
  const doc = s.snapshot();
  doc.layers[0].nodes.find((n) => n.id === b.id).params.value.expression = 'value + 1';
  assert.throws(() => deserialize(JSON.stringify(doc)), /schemaVersion 26/);
  const v = fixture('Vector');
  assert.throws(() => v.send('set-parameter-expression', { expression: 'value + 10' }), /Vector2/);
  assert.throws(
    () => v.send('set-parameter-expression', { expression: `ref("${v.a.id}", "value") + value` }),
    /Scalar|Vector2/,
  );
});

test('legacy 24/25 files retain version; only bare value syntax promotes to 26 including dormant expressions', () => {
  for (const version of [24, 25]) {
    const doc = document(),
      l = doc.layers[0],
      a = node('Value'),
      b = node('Value');
    doc.schemaVersion = version;
    l.nodes.push(a, b);
    const s = new CommandSession(deserialize(serialize(doc)));
    const send = (type, payload) =>
      s.dispatch({
        ...s.state,
        type,
        payload: { layerId: l.id, nodeId: b.id, key: 'value', ...payload },
      });
    send('set-parameter-expression', { expression: `ref("${a.id}", "value") + 1` });
    assert.equal(deserialize(serialize(s.snapshot())).schemaVersion, version);
    send('set-parameter-base', { value: 7 });
    assert.equal(s.snapshot().schemaVersion, version);
    send('connect', { sourceId: a.id });
    send('set-parameter-expression', { expression: 'value + 1' });
    const restored = deserialize(serialize(s.snapshot()));
    assert.equal(restored.schemaVersion, 26);
    const q = restored.layers[0].nodes.find((n) => n.id === b.id).params.value;
    assert.equal(q.value, 7);
    assert.equal(q.expression, 'value + 1');
    assert.equal(q.source, a.id);
    const old = structuredClone(restored);
    old.schemaVersion = version;
    assert.throws(() => deserialize(JSON.stringify(old)), /schemaVersion 26/);
  }
});
test('composition lock and unsupported Boolean base editing reject without changes', () => {
  const doc = document(),
    l = doc.layers[0],
    n = node('Toggle');
  l.nodes.push(n);
  const s = new CommandSession(doc),
    before = serialize(s.snapshot());
  assert.throws(
    () =>
      s.dispatch({
        ...s.state,
        type: 'set-parameter-base',
        payload: { layerId: l.id, nodeId: n.id, key: 'value', value: true },
      }),
    /Scalar, Vector2 or Color/,
  );
  assert.equal(serialize(s.snapshot()), before);
  const f = fixture();
  const locked = f.s.snapshot();
  locked.layers[0].locked = true;
  const sl = new CommandSession(locked);
  assert.throws(
    () =>
      sl.dispatch({
        ...sl.state,
        type: 'set-parameter-base',
        payload: { layerId: locked.layers[0].id, nodeId: f.b.id, key: 'value', value: 1 },
      }),
    /locked/,
  );
});

test('Text position cache tracks base and referenced changes without outline evaluation', () => {
  const doc = document(),
    l = doc.layers[0],
    a = node('Value'),
    t = node('Text');
  doc.schemaVersion = 26;
  l.nodes.push(a, t);
  a.params.value.value = 2;
  t.params.position.value = [20, 10];
  t.params.position.expression = `ref("${a.id}", "value") + value; value`;
  assert.deepEqual(evaluate(l).run(t.id, 'position'), [22, 10]);
  t.params.position.value[0] = 30;
  assert.deepEqual(evaluate(l).run(t.id, 'position'), [32, 10]);
  a.params.value.value = 5;
  assert.deepEqual(evaluate(l).run(t.id, 'position'), [35, 10]);
});
