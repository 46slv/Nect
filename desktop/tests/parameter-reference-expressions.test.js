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

for (const whitespace of ['', ' ', '\t', '\r\n', '\u00a0']) {
  test(`group duplication remaps ref with ${JSON.stringify(whitespace)} before parentheses`, () => {
    const { doc, l, scalar, sink } = fixture();
    scalar.params.value.value = 3;
    const original = `ref${whitespace}(\n "${scalar.id}" , "value" )`;
    sink.params.value.expression = original;
    assert.equal(evaluate(l).run(sink.id), 3);
    const [copyScalar, copySink] = duplicateNodes(l, [scalar.id, sink.id]);
    copyScalar.params.value.value = 7;
    assert.equal(copySink.params.value.expression, original.replace(scalar.id, copyScalar.id));
    assert.equal(sink.params.value.expression, original);
    assert.equal(evaluate(l).run(copySink.id), 7);
    assert.equal(evaluate(l).run(sink.id), 3);
    const restored = deserialize(serialize(doc)).layers[0];
    assert.equal(evaluate(restored).run(copySink.id), 7);
    assert.equal(evaluate(restored).run(sink.id), 3);
  });
}

test('duplicate repeated vector refs exactly, preserving external and dangling IDs', () => {
  const { doc, l, source, scalar, sink } = fixture();
  scalar.id = 'node-ref';
  sink.id = 'node-ref-long';
  sink.params.value.value = 10;
  const internal = `ref \n("${scalar.id}", "value")`,
    external = `ref\t("${sink.id}", "value")`,
    original = `${internal} + ${internal}; ${external} + ${internal}`;
  source.params.value.expression = original;
  const [copySource, copyScalar] = duplicateNodes(l, [source.id, scalar.id]);
  copyScalar.params.value.value = 7;
  assert.equal(
    copySource.params.value.expression,
    original.replaceAll(`"${scalar.id}"`, `"${copyScalar.id}"`),
  );
  assert.deepEqual(evaluate(l).run(copySource.id), [14, 17]);
  assert.deepEqual(evaluate(l).run(source.id), [8, 14]);
  const [externalCopy] = duplicateNodes(l, [source.id]);
  assert.equal(externalCopy.params.value.expression, original);
  const restored = deserialize(serialize(doc)).layers[0];
  assert.deepEqual(evaluate(restored).run(copySource.id), [14, 17]);
  assert.deepEqual(evaluate(restored).run(externalCopy.id), [8, 14]);
  sink.params.value.expression = 'ref \n("node-missing", "value")';
  const [danglingCopy] = duplicateNodes(l, [sink.id]);
  assert.equal(danglingCopy.params.value.expression, sink.params.value.expression);
  assert.throws(() => evaluate(l).run(danglingCopy.id), /missing/);
  assert.doesNotThrow(() => deserialize(serialize(doc)));
});

test('duplicate dormant component refs while retaining connected socket priority', () => {
  const { doc, l, source, scalar, sink } = fixture();
  connect(l, scalar.id, sink.id, 'value');
  const original = `ref \n("${source.id}", "value", "y")`;
  sink.params.value.expression = original;
  const [copySource, copyScalar, copySink] = duplicateNodes(l, [source.id, scalar.id, sink.id]);
  copySource.params.value.value = [300, 500];
  copyScalar.params.value.value = 9;
  assert.equal(copySink.params.value.expression, original.replace(source.id, copySource.id));
  assert.equal(copySink.params.value.source, copyScalar.id);
  assert.equal(evaluate(l).run(copySink.id), 9);
  const restored = deserialize(serialize(doc)).layers[0],
    restoredSink = restored.nodes.find((n) => n.id === copySink.id);
  assert.equal(evaluate(restored).run(copySink.id), 9);
  restoredSink.params.value.source = null;
  assert.equal(evaluate(restored).run(copySink.id), 500);
  assert.equal(evaluate(l).run(sink.id), 4);
  assert.equal(sink.params.value.expression, original);
});

test('spaced ref duplication is one undoable command; malformed drafts remain atomic', () => {
  const { doc, l, scalar, sink } = fixture();
  sink.params.value.expression = `ref ("${scalar.id}", "value")`;
  const session = new CommandSession(doc),
    dispatch = (type, payload = {}) =>
      session.dispatch({ ...session.state, type, payload: { layerId: l.id, ...payload } }),
    before = serialize(session.snapshot());
  dispatch('duplicate-nodes', { nodeIds: [scalar.id, sink.id] });
  const after = serialize(session.snapshot()),
    copies = session.snapshot().layers[0].nodes.slice(-2);
  assert.equal(copies[1].params.value.expression, `ref ("${copies[0].id}", "value")`);
  dispatch('undo');
  assert.equal(serialize(session.snapshot()), before);
  dispatch('redo');
  assert.equal(serialize(session.snapshot()), after);
  for (const expression of [
    `xref ("${scalar.id}", "value")`,
    `ref ("${scalar.id}", "value").constructor`,
    `ref ("${scalar.id}", "value", "z")`,
    `ref ("${scalar.id}", "value"`,
    `ref ('${scalar.id}', 'value')`,
    `ref ("${scalar.id}", "va\\"lue")`,
  ]) {
    const revision = session.state.expectedRevision;
    assert.throws(() =>
      dispatch('set-parameter-expression', { nodeId: sink.id, key: 'value', expression }),
    );
    assert.equal(serialize(session.snapshot()), after);
    assert.equal(session.state.expectedRevision, revision);
  }
});
