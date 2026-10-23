import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  layer,
  connect,
  evaluate,
  CommandSession,
  serialize,
  deserialize,
  parameterReferenceExpression as ref,
  expressionReferenceTargets,
  duplicateNodes,
} from '../src/core.js';

function fixture(type = 'Fill', key = 'color') {
  const doc = document(),
    l = doc.layers[0],
    a = node('Color'),
    b = node(type);
  a.params.value.value = { hex: '#123456', alpha: 0.25 };
  if (type === 'Keyer') doc.schemaVersion = 35;
  b.params[key].value = { hex: '#abcdef', alpha: 0.6 };
  l.nodes.push(a, b);
  const s = new CommandSession(doc);
  const send = (type, payload = {}) =>
    s.dispatch({ ...s.state, type, payload: { layerId: l.id, nodeId: b.id, key, ...payload } });
  const live = () => s.snapshot().layers[0];
  const q = () => live().nodes.find((n) => n.id === b.id).params[key];
  const result = () => evaluate(live()).parameterValue(b.id, key);
  return { doc, l, a, b, s, send, live, q, result, key };
}

for (const [type, key] of [
  ['Color', 'value'],
  ['Fill', 'color'],
  ['Stroke', 'color'],
  ['Keyer', 'keyColor'],
  ['Linear Gradient Fill', 'startColor'],
  ['Linear Gradient Fill', 'endColor'],
  ['Radial Gradient Fill', 'startColor'],
  ['Radial Gradient Fill', 'endColor'],
]) {
  test(`${type}.${key} reads RGBA base without accumulation; one Undo and reopen retain expression`, () => {
    const f = fixture(type, key);
    f.send('set-parameter-expression', { expression: 'value' });
    assert.equal(f.s.snapshot().schemaVersion, 42);
    for (let i = 0; i < 100; i++) assert.deepEqual(f.result(), f.q().value);
    const before = serialize(f.s.snapshot()),
      revision = f.s.state.expectedRevision;
    f.send('set-parameter-base', { value: { hex: '#987654', alpha: 0 } });
    assert.equal(f.s.state.expectedRevision, revision + 1);
    assert.equal(f.q().expression, 'value');
    assert.deepEqual(f.result(), { hex: '#987654', alpha: 0 });
    const saved = serialize(f.s.snapshot());
    f.send('undo');
    assert.equal(serialize(f.s.snapshot()), before);
    f.send('redo');
    assert.equal(serialize(f.s.snapshot()), saved);
    const restored = deserialize(saved);
    assert.deepEqual(evaluate(restored.layers[0]).parameterValue(f.b.id, key), f.result());
    const state = f.s.state;
    f.send('set-parameter-base', { value: f.q().value });
    assert.deepEqual(f.s.state, state, 'unchanged base is a no-op');
    assert.throws(() => f.send('set-parameter', { value: { hex: '#ffffff', alpha: 1 } }), /driven/);
  });
}

test('typed effective Color references survive rename/reorder, update dependencies and duplicate by ID', () => {
  const f = fixture('Linear Gradient Fill', 'startColor');
  f.send('set-parameter-expression', { nodeId: f.a.id, key: 'value', expression: 'value' });
  f.send('set-parameter-expression', { expression: ref(f.a.id, 'value') });
  assert.deepEqual(f.result(), f.a.params.value.value);
  f.send('set-parameter-base', {
    nodeId: f.a.id,
    key: 'value',
    value: { hex: '#ff0000', alpha: 0.4 },
  });
  assert.deepEqual(f.result(), { hex: '#ff0000', alpha: 0.4 });
  f.send('rename', { nodeId: f.a.id, name: 'Renamed Color' });
  const d = deserialize(serialize(f.s.snapshot())),
    l = d.layers[0];
  l.nodes.reverse();
  const copies = duplicateNodes(l, [f.a.id, f.b.id]);
  const ca = copies.find((n) => n.type === 'Color'),
    cb = copies.find((n) => n.type === 'Linear Gradient Fill');
  ca.params.value.value = { hex: '#00ff00', alpha: 0.8 };
  assert.equal(cb.params.startColor.expression, ref(ca.id, 'value'));
  assert.deepEqual(evaluate(l).parameterValue(cb.id, 'startColor'), ca.params.value.value);
  assert.deepEqual(evaluate(l).parameterValue(f.b.id, 'startColor'), f.result());
  assert.doesNotThrow(() => deserialize(serialize(d)));
  const targets = expressionReferenceTargets(l, f.b.id, 'Color');
  assert.ok(targets.some((t) => t.nodeId === f.a.id && t.type === 'Color'));
  assert.ok(targets.every((t) => t.type === 'Color' && !t.component && t.nodeId !== f.b.id));
  assert.ok(!expressionReferenceTargets(l, f.b.id).some((t) => t.nodeId === f.a.id));
});

test('Color dependency cache follows effective reference and preserves unrelated numeric reuse', () => {
  const f = fixture('Color', 'value'),
    l = f.doc.layers[0],
    scalar = node('Value');
  f.doc.schemaVersion = 42;
  l.nodes.push(scalar);
  f.b.params.value.expression = ref(f.a.id, 'value');
  const run = () => {
    const e = evaluate(l);
    e.run(scalar.id);
    e.run(f.a.id);
    assert.deepEqual(e.run(f.b.id), f.a.params.value.value);
    return e;
  };
  run();
  const same = run();
  assert.equal(same.stats.computed, 0);
  f.a.params.value.value = { hex: '#222222', alpha: 0.3 };
  const changed = run();
  assert.ok(changed.stats.reused >= 1);
});

test('errors, stale requests, invalid base, type mismatch and cycles reject atomically', () => {
  const f = fixture(),
    scalar = node('Value');
  f.doc.layers[0].nodes.push(scalar);
  const s = new CommandSession(f.doc);
  const send = (type, payload = {}) =>
    s.dispatch({
      ...s.state,
      type,
      payload: { layerId: f.l.id, nodeId: f.b.id, key: 'color', ...payload },
    });
  const rejects = (type, payload, pattern) => {
    const before = serialize(s.snapshot()),
      state = s.state,
      history = s.historyView.past.length;
    assert.throws(() => send(type, payload), pattern);
    assert.equal(serialize(s.snapshot()), before);
    assert.deepEqual(s.state, state);
    assert.equal(s.historyView.past.length, history);
  };
  for (const expression of [
    '1',
    'value + 1',
    'alpha(1, 0.5)',
    'value; value',
    'value.hex',
    'v a l u e',
    'globalThis.alert(1)',
    'value'.repeat(1025),
  ])
    rejects('set-parameter-expression', { expression }, /Color|4096/);
  rejects('set-parameter-expression', { expression: ref('missing-color', 'value') }, /missing/);
  rejects('set-parameter-expression', { expression: ref(scalar.id, 'value') }, /Color/);
  rejects('set-parameter-expression', { expression: ref(f.a.id, 'missing') }, /Color/);
  rejects('set-parameter-expression', { expression: ref(f.b.id, 'color') }, /cycle/i);
  rejects(
    'set-parameter-expression',
    { nodeId: scalar.id, key: 'value', expression: ref(f.a.id, 'value') },
    /Scalar/,
  );
  send('set-parameter-expression', { expression: ref(f.a.id, 'value') });
  rejects(
    'set-parameter-expression',
    { nodeId: f.a.id, key: 'value', expression: ref(f.b.id, 'color') },
    /cycle/i,
  );
  for (const value of [
    null,
    [1, 0, 0, 1],
    { hex: '#gg0000', alpha: 1 },
    { hex: '#ffffff', alpha: 1.1 },
    { hex: '#ffffff', alpha: NaN },
    { hex: '#ffffff', alpha: 1, extra: true },
  ])
    rejects('set-parameter-base', { value }, /Color/);
  const stale = s.state;
  send('set-parameter-base', { value: { hex: '#000000', alpha: 1 } });
  const before = serialize(s.snapshot());
  assert.throws(
    () =>
      s.dispatch({
        ...stale,
        type: 'set-parameter-base',
        payload: {
          layerId: f.l.id,
          nodeId: f.b.id,
          key: 'color',
          value: { hex: '#ffffff', alpha: 1 },
        },
      }),
    /STALE/,
  );
  assert.equal(serialize(s.snapshot()), before);
  send('set-parameter-lock', { locked: true });
  rejects('set-parameter-base', { value: { hex: '#ffffff', alpha: 1 } }, /locked/);
});

test('source priority and dormant expression/base survive disconnect; deleted targets remain diagnosed and Undo restores', () => {
  const f = fixture();
  f.send('set-parameter-expression', { expression: 'value' });
  f.send('connect', { sourceId: f.a.id });
  f.send('set-parameter-base', { value: { hex: '#998877', alpha: 0.15 } });
  assert.deepEqual(f.result(), f.a.params.value.value);
  assert.equal(f.q().expression, 'value');
  f.send('disconnect');
  assert.deepEqual(f.result(), f.q().value);
  f.send('set-parameter-expression', { expression: ref(f.a.id, 'value') });
  const before = serialize(f.s.snapshot());
  f.send('delete-node', { nodeId: f.a.id });
  assert.throws(f.result, /missing/);
  assert.doesNotThrow(() => deserialize(serialize(f.s.snapshot())));
  f.send('undo');
  assert.equal(serialize(f.s.snapshot()), before);
});

test('live legacy Fill base, schema gating, cross-composition references and composition locks', () => {
  const f = fixture();
  f.doc.layers[0].fill = '#445566';
  f.b.params.color.value = null;
  f.b.params.color.expression = 'value';
  assert.throws(() => deserialize(serialize(f.doc)), /schemaVersion 42/);
  f.doc.schemaVersion = 42;
  assert.deepEqual(evaluate(f.l).parameterValue(f.b.id, 'color'), { hex: '#445566', alpha: 1 });
  f.l.fill = '#112233';
  assert.deepEqual(evaluate(f.l).parameterValue(f.b.id, 'color'), { hex: '#112233', alpha: 1 });
  const other = layer();
  f.doc.layers.push(other);
  f.l.nodes = f.l.nodes.filter((n) => n.id !== f.a.id);
  other.nodes.push(f.a);
  f.b.params.color.expression = ref(f.a.id, 'value');
  assert.throws(() => deserialize(serialize(f.doc)), /same layer/);
  f.b.params.color.expression = 'value';
  f.l.locked = true;
  const s = new CommandSession(f.doc),
    before = serialize(s.snapshot());
  assert.throws(
    () =>
      s.dispatch({
        ...s.state,
        type: 'set-parameter-base',
        payload: {
          layerId: f.l.id,
          nodeId: f.b.id,
          key: 'color',
          value: { hex: '#123456', alpha: 0.5 },
        },
      }),
    /locked/,
  );
  assert.equal(serialize(s.snapshot()), before);
});

test('named presets retain typed Color expressions and remap internal references', () => {
  const f = fixture('Color', 'value');
  f.send('set-parameter-expression', { expression: ref(f.a.id, 'value') });
  const { presetId } = f.send('save-node-preset', {
    nodeIds: [f.a.id, f.b.id],
    name: 'Color pair',
  });
  const saved = serialize(f.s.snapshot());
  assert.doesNotThrow(() => deserialize(saved));
  const { nodeIds } = f.send('apply-node-preset', { presetId });
  const l = f.live(),
    copies = l.nodes.filter((n) => nodeIds.includes(n.id));
  const target = copies.find((n) => n.params.value.expression);
  const source = copies.find((n) => !n.params.value.expression);
  assert.equal(target.params.value.expression, ref(source.id, 'value'));
  assert.deepEqual(evaluate(l).run(target.id), source.params.value.value);
});
