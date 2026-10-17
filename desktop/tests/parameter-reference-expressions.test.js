import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  layer,
  connect,
  evaluate,
  expression,
  expressionReferences,
  expressionReferenceTargets,
  parameterReferenceExpression as ref,
  serialize,
  deserialize,
  duplicateNodes,
  CommandSession,
} from '../src/core.js';
function fixture() {
  const doc = document(),
    l = doc.layers[0],
    source = node('Vector'),
    scalar = node('Value'),
    sink = node('Value');
  source.params.value.value = [100, 200];
  scalar.params.value.value = 4;
  l.nodes.push(source, scalar, sink);
  return { doc, l, source, scalar, sink };
}
test('safe references combine scalar/component parameters with arithmetic and existing functions', () => {
  const { doc, l, source, scalar, sink } = fixture();
  sink.params.value.expression = `${ref(source.id, 'value', 'x')} * 0.5 + ${ref(scalar.id, 'value')} + sin(pi / 2)`;
  assert.equal(evaluate(l).run(sink.id), 55);
  source.params.value.expression = '300; 400';
  assert.equal(evaluate(l).run(sink.id), 155);
  scalar.params.value.expression = '8';
  assert.equal(evaluate(l).run(sink.id), 159);
  const restored = deserialize(serialize(doc)).layers[0];
  assert.equal(evaluate(restored).run(sink.id), 159);
});
test('references read effective connected values, without evaluating unrelated Text geometry', () => {
  const { l, source, sink } = fixture(),
    text = node('Text');
  l.nodes.push(text);
  connect(l, source.id, text.id, 'position');
  sink.params.value.expression = ref(text.id, 'position', 'y');
  assert.equal(evaluate(l).run(sink.id), 200);
  source.params.value.value[1] = 555;
  assert.equal(evaluate(l).run(sink.id), 555);
});
test('vector expressions support independent references and duplicate groups retarget IDs', () => {
  const { doc, l, source, scalar } = fixture();
  source.params.value.expression = `${ref(scalar.id, 'value')} * 2; ${ref(scalar.id, 'value')} + 10`;
  assert.deepEqual(evaluate(l).run(source.id), [8, 14]);
  const [copySource, copyScalar] = duplicateNodes(l, [source.id, scalar.id]);
  assert.ok(copySource.params.value.expression.includes(copyScalar.id));
  copyScalar.params.value.value = 9;
  assert.deepEqual(evaluate(l).run(copySource.id), [18, 19]);
  assert.deepEqual(evaluate(l).run(source.id), [8, 14]);
  serialize(doc);
});
test('unsafe syntax, nonnumeric targets, cross-layer references and cycles reject', () => {
  const { doc, l, source, scalar, sink } = fixture();
  for (const expr of [
    'ref("x", "__proto__")',
    'ref("x", "value").constructor',
    'ref("x", "value", "z")',
    'globalThis.alert(1)',
    'constructor(1)',
    'ref("x", "value"); 2',
  ])
    assert.throws(() => expression(expr));
  sink.params.value.expression = ref(source.id, 'value');
  assert.throws(() => deserialize(serialize(doc)), /Scalar/);
  sink.params.value.expression = ref(scalar.id, 'value');
  scalar.params.value.expression = ref(sink.id, 'value');
  assert.throws(() => deserialize(serialize(doc)), /cycle/i);
  assert.throws(() => evaluate(l).run(sink.id), /cycle/i);
  scalar.params.value.expression = '';
  const other = layer();
  doc.layers.push(other);
  l.nodes = l.nodes.filter((n) => n !== scalar);
  other.nodes.push(scalar);
  assert.throws(() => deserialize(serialize(doc)), /same layer/);
});
test('picker excludes current/downstream nodes and exposes only numeric components', () => {
  const { l, source, scalar, sink } = fixture();
  sink.params.value.expression = ref(scalar.id, 'value');
  const targets = expressionReferenceTargets(l, scalar.id);
  assert.ok(targets.some((t) => t.nodeId === source.id && t.component === 'x'));
  assert.ok(!targets.some((t) => [scalar.id, sink.id].includes(t.nodeId)));
  assert.deepEqual(expressionReferences(`1 / (${ref(source.id, 'value', 'x')} - 0)`), [
    { nodeId: source.id, key: 'value', component: 'x' },
  ]);
});
test('commands enforce locks and rollback nonfinite references; undo/redo and dangling delete survive', () => {
  const { doc, l, scalar, sink } = fixture(),
    session = new CommandSession(doc);
  const dispatch = (type, payload = {}) =>
    session.dispatch({ ...session.state, type, payload: { layerId: l.id, ...payload } });
  dispatch('set-parameter-expression', {
    nodeId: sink.id,
    key: 'value',
    expression: `${ref(scalar.id, 'value')} * 2`,
  });
  assert.equal(evaluate(session.snapshot().layers[0]).run(sink.id), 8);
  const before = serialize(session.snapshot());
  assert.throws(
    () =>
      dispatch('set-parameter-expression', {
        nodeId: sink.id,
        key: 'value',
        expression: `1 / (${ref(scalar.id, 'value')} - 4)`,
      }),
    /finite/,
  );
  assert.equal(serialize(session.snapshot()), before);
  dispatch('set-parameter-lock', { nodeId: sink.id, key: 'value', locked: true });
  assert.throws(
    () => dispatch('set-parameter-expression', { nodeId: sink.id, key: 'value', expression: '1' }),
    /locked/,
  );
  dispatch('undo');
  dispatch('delete-node', { nodeId: scalar.id });
  const after = deserialize(serialize(session.snapshot()));
  assert.throws(() => evaluate(after.layers[0]).run(sink.id), /missing/);
  dispatch('undo');
  assert.equal(evaluate(session.snapshot().layers[0]).run(sink.id), 8);
  dispatch('redo');
  assert.throws(() => evaluate(session.snapshot().layers[0]).run(sink.id), /missing/);
});
test('Text position-output cache tracks expression dependencies independently of outline generation', () => {
  const { l, scalar } = fixture(),
    text = node('Text');
  l.nodes.push(text);
  text.params.position.expression = `${ref(scalar.id, 'value')}; ${ref(scalar.id, 'value')} * 2`;
  assert.deepEqual(evaluate(l).run(text.id, 'position'), [4, 8]);
  scalar.params.value.value = 7;
  assert.deepEqual(evaluate(l).run(text.id, 'position'), [7, 14]);
});
test('picker safely ignores Curve point descriptors and rejects point-reference targets', () => {
  const { doc, l, sink } = fixture(),
    curve = node('Curve');
  l.nodes.push(curve);
  assert.doesNotThrow(() => expressionReferenceTargets(l, sink.id));
  sink.params.value.expression = ref(curve.id, `${curve.points[0].id}.position`, 'x');
  assert.throws(() => deserialize(serialize(doc)), /Scalar/);
});
test('connected targets still reject invalid authored expressions without changing socket priority', () => {
  const { doc, l, scalar, sink } = fixture();
  connect(l, scalar.id, sink.id, 'value');
  const s = new CommandSession(doc),
    send = (value) =>
      s.dispatch({
        ...s.state,
        type: 'set-parameter-expression',
        payload: { layerId: l.id, nodeId: sink.id, key: 'value', expression: value },
      });
  assert.throws(() => send('bad syntax'));
  send(`${ref(scalar.id, 'value')} * 3`);
  assert.equal(evaluate(s.snapshot().layers[0]).run(sink.id), 4);
});
