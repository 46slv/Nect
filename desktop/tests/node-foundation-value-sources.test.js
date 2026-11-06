import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  definitions,
  activeParameters,
  outputPorts,
  outputType,
  operationOptions,
  bypassContract,
  connect,
  evaluate,
  serialize,
  deserialize,
  CommandSession,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { evaluateValueSource } from '../src/value-source-binding.js';

const types = ['Value', 'Vector', 'Expression'];
const scalar = (type) => type !== 'Vector';
const value = (type, a, b) => (scalar(type) ? a : [a, b]);
function scene(type) {
  const doc = document(),
    layer = doc.layers[0],
    source = node(type),
    target = node(type);
  layer.nodes.push(source, target);
  return { doc, layer, source, target };
}

test('registered value sources preserve defaults, legacy output, factory fields and placement', () => {
  for (const type of types) {
    const resolved = builtinNodeRegistry.get(type),
      n = node(type),
      other = node(type),
      expected = value(type, scalar(type) ? 100 : 300, 250);
    assert.ok(resolved);
    assert.ok(Object.isFrozen(resolved.declaration));
    assert.deepEqual(n.params.value, {
      type: scalar(type) ? 'Scalar' : 'Vector2',
      value: expected,
      expression: '',
      locked: false,
    });
    assert.equal(n.name, type);
    assert.equal(n.operation, 'subtract');
    assert.deepEqual(
      activeParameters(n).map(([id]) => id),
      ['value'],
    );
    assert.deepEqual(outputPorts(n), [
      { id: undefined, label: '', type: scalar(type) ? 'Scalar' : 'Vector2' },
    ]);
    assert.equal(outputType(n), scalar(type) ? 'Scalar' : 'Vector2');
    for (const port of ['default', 'output', 'x']) assert.equal(outputType(n, port), undefined);
    assert.equal(operationOptions[type], undefined);
    assert.equal(bypassContract(n).supported, false);
    if (type === 'Vector') {
      assert.equal(n.canvasPositionHandle, false);
      assert.ok(Object.isFrozen(resolved.declaration.parameters[0].default));
      n.params.value.value[0] = -1;
    } else n.params.value.value = -1;
    assert.deepEqual(other.params.value.value, expected);
    assert.deepEqual(definitions[type].value.value, expected);
  }
  const definitionsOrder = Object.keys(definitions),
    catalogOrder = nodeCatalog.map(({ typeId }) => typeId);
  for (const [type, definitionAfter, catalogAfter] of [
    ['Value', 'Instance on Points', 'Line Screen'],
    ['Vector', 'Color', 'Math'],
    ['Expression', 'Composition Reference', 'Reference'],
  ]) {
    assert.equal(definitionsOrder[definitionsOrder.indexOf(type) - 1], definitionAfter);
    assert.equal(catalogOrder[catalogOrder.indexOf(type) - 1], catalogAfter);
  }
});

test('one trusted value binding is reused without adding type-specific evaluator behavior', () => {
  const rows = builtinManifest.filter((row) => types.includes(row.declaration.typeId));
  assert.equal(rows.length, 3);
  assert.equal(new Set(rows.map((row) => row.declaration.implementationId)).size, 1);
  assert.ok(rows.every((row) => row.binding.evaluate === evaluateValueSource));
  const vector = [3, 7],
    reads = [];
  assert.equal(
    evaluateValueSource(undefined, (key) => {
      reads.push(key);
      return vector;
    }),
    vector,
  );
  assert.deepEqual(reads, ['value']);
  for (const type of types) {
    const resolved = builtinNodeRegistry.get(type),
      expected = value(type, 3, 7);
    assert.deepEqual(
      resolved.evaluate('subtract', (key) => {
        assert.equal(key, 'value');
        return expected;
      }),
      expected,
    );
  }
});

for (const type of types)
  test(`${type} Session preserves source, expression, base, atomic failure, history and reload`, () => {
    const { doc, layer, source, target } = scene(type),
      wrong = node(scalar(type) ? 'Vector' : 'Value');
    layer.nodes.push(wrong);
    doc.schemaVersion = 26;
    source.params.value.value = value(type, 3, 7);
    target.params.value.value = value(type, 11, -5);
    target.params.value.expression = scalar(type) ? 'value * 2' : 'value * 2; value * 3';
    connect(layer, source.id, target.id, 'value');
    const session = new CommandSession(doc);
    const dispatch = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: layer.id, nodeId: target.id, key: 'value', ...payload },
      });
    const run = () => evaluate(session.snapshot().layers[0]).run(target.id);
    const before = serialize(session.snapshot());
    assert.deepEqual(run(), value(type, 3, 7));
    dispatch('disconnect');
    const expression = serialize(session.snapshot());
    assert.deepEqual(run(), value(type, 22, -15));
    assert.equal(session.historyView.past.length, 1);
    dispatch('undo');
    assert.equal(serialize(session.snapshot()), before);
    const revision = session.state.expectedRevision,
      history = JSON.stringify(session.historyView);
    for (const payload of [{ sourceId: wrong.id }, { sourceId: target.id, nodeId: source.id }]) {
      assert.throws(() => dispatch('connect', payload), /connect|cycle/i);
      assert.equal(serialize(session.snapshot()), before);
      assert.equal(session.state.expectedRevision, revision);
      assert.equal(JSON.stringify(session.historyView), history);
    }
    dispatch('redo');
    assert.equal(serialize(session.snapshot()), expression);
    dispatch('set-parameter-expression', { expression: '' });
    assert.deepEqual(run(), value(type, 11, -5));
    dispatch('set-parameter', { value: value(type, 19, -21) });
    const after = serialize(session.snapshot());
    dispatch('undo');
    assert.deepEqual(run(), value(type, 11, -5));
    dispatch('redo');
    assert.equal(serialize(session.snapshot()), after);
    const loaded = deserialize(after);
    assert.equal(serialize(loaded), after);
    assert.deepEqual(evaluate(loaded.layers[0]).run(target.id), value(type, 19, -21));
    assert.equal(loaded.schemaVersion, 26);
    assert.doesNotMatch(after, /implementationId|capabilities|legacyDefault|builtin.read-value/);
  });

test('value-source cache reuse and invalidation preserve detached prior results', () => {
  for (const type of types) {
    const { layer, source, target } = scene(type);
    source.params.value.value = value(type, 3, 7);
    layer.nodes = [source, target];
    layer.output = target.id;
    connect(layer, source.id, target.id, 'value');
    const first = evaluate(layer);
    assert.deepEqual(first.path, value(type, 3, 7));
    assert.deepEqual(first.stats, { computed: 2, reused: 0 });
    assert.deepEqual(evaluate(layer).stats, { computed: 0, reused: 2 });
    source.params.value.value = value(type, 4, 8);
    const changed = evaluate(layer);
    assert.deepEqual(changed.path, value(type, 4, 8));
    assert.deepEqual(changed.stats, { computed: 2, reused: 0 });
    assert.deepEqual(first.path, value(type, 3, 7));
  }
});

test('registered source nodes still reject nonfinite expressions and named legacy ports', () => {
  for (const type of types) {
    const { layer, target } = scene(type),
      engine = evaluate(layer);
    for (const port of ['default', 'output', 'x'])
      assert.throws(() => engine.run(target.id, port), /socket/);
    target.params.value.expression = scalar(type) ? '1/0' : '1/0; 0';
    assert.throws(() => evaluate(layer).run(target.id), /finite/);
  }
});
