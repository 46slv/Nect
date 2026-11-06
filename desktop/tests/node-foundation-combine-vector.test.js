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
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog, nodePickerItems } from '../src/node-catalog.js';
import { combineVectorDeclaration } from '../src/combine-vector-declaration.js';
import { evaluateCombineVector } from '../src/combine-vector-binding.js';
import { createNodeRegistry } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';

const registered = builtinNodeRegistry.get('Combine Vector');
const scene = () => {
  const doc = document(),
    layer = doc.layers[0],
    combine = node('Combine Vector');
  layer.nodes.push(combine);
  return { doc, layer, combine };
};

test('Combine Vector has one registered declaration and preserves factory and legacy ports', () => {
  const { combine } = scene(),
    other = node('Combine Vector');
  assert.ok(registered);
  assert.ok(Object.isFrozen(registered.declaration));
  assert.equal(combine.type, 'Combine Vector');
  assert.equal(combine.name, 'Combine Vector');
  assert.equal(combine.operation, 'subtract');
  assert.deepEqual(Object.keys(combine.params), ['x', 'y']);
  assert.deepEqual(combine.params.x, { type: 'Scalar', value: 0, expression: '', locked: false });
  assert.deepEqual(combine.params.y, { type: 'Scalar', value: 0, expression: '', locked: false });
  combine.params.x.value = 9;
  assert.equal(other.params.x.value, 0);
  assert.equal(definitions['Combine Vector'].x.value, 0);
  assert.deepEqual(
    activeParameters(combine).map(([id]) => id),
    ['x', 'y'],
  );
  assert.deepEqual(outputPorts(combine), [{ id: undefined, label: '', type: 'Vector2' }]);
  assert.equal(outputType(combine), 'Vector2');
  for (const port of ['default', 'output', 'x', 'y'])
    assert.equal(outputType(combine, port), undefined);
  assert.equal(operationOptions['Combine Vector'], undefined);
  assert.equal(bypassContract(combine).supported, false);
  const types = Object.keys(definitions);
  assert.equal(types[types.indexOf('Separate Vector') + 1], 'Combine Vector');
  const items = nodePickerItems(definitions),
    index = items.findIndex(({ typeId }) => typeId === combine.type);
  const catalogIndex = nodeCatalog.findIndex(({ typeId }) => typeId === combine.type);
  assert.equal(nodeCatalog[catalogIndex - 1].typeId, 'Separate Vector');
  assert.equal(nodeCatalog[catalogIndex + 1].typeId, 'Toggle');
  assert.equal(items[index - 1].typeId, 'Color');
  assert.equal(items[index + 1].typeId, 'Composition Reference');
  assert.deepEqual(items[index].aliases, ['ベクトル合成']);
  assert.equal(items[index].labelJa, registered.declaration.catalog.labelJa);
});

test('trusted Combine binding reads X then Y and registry captures it without operation semantics', () => {
  const reads = [];
  const result = evaluateCombineVector(undefined, (id) => {
    reads.push(id);
    return id === 'x' ? -3 : 8;
  });
  assert.deepEqual(result, [-3, 8]);
  assert.deepEqual(reads, ['x', 'y']);
  assert.deepEqual(
    registered.evaluate('subtract', (id) => (id === 'x' ? Number.MIN_VALUE : Number.MAX_VALUE)),
    [Number.MIN_VALUE, Number.MAX_VALUE],
  );
  const input = structuredClone(combineVectorDeclaration);
  const binding = { evaluate: evaluateCombineVector };
  const bindings = new Map([[input.implementationId, binding]]);
  const resolved = createNodeRegistry([input], bindings, nectNodeHost).get(input.typeId);
  input.parameters[0].default = 99;
  binding.evaluate = () => [99, 99];
  assert.equal(resolved.declaration.parameters[0].default, 0);
  assert.deepEqual(
    resolved.evaluate(undefined, () => 4),
    [4, 4],
  );
  const invalid = structuredClone(combineVectorDeclaration);
  invalid.parameters[0].default = [0, 0];
  assert.throws(() => createNodeRegistry([invalid], bindings, nectNodeHost), /default/);
});

test('Combine evaluation retains source priority, independent fallback and invalid port refusal', () => {
  const { doc, layer, combine } = scene();
  doc.schemaVersion = 26;
  const value = node('Value'),
    separate = node('Separate Vector');
  layer.nodes.push(value, separate);
  combine.params.x.value = 11;
  combine.params.x.expression = 'value * 2';
  combine.params.y.value = -5;
  value.params.value.value = 17;
  separate.params.vector.value = [25, -13];
  connect(layer, value.id, combine.id, 'x');
  connect(layer, separate.id, combine.id, 'y', false, 'y');
  const before = serialize(doc);
  const engine = evaluate(layer);
  assert.deepEqual(engine.run(combine.id), [17, -13]);
  for (const port of ['default', 'output', 'x', 'y'])
    assert.throws(() => engine.run(combine.id, port), /socket/);
  assert.throws(() => connect(layer, separate.id, combine.id, 'x'), /connect/i);
  assert.equal(serialize(doc), before);
  const saved = deserialize(before);
  assert.equal(serialize(saved), before);
  assert.deepEqual(evaluate(saved.layers[0]).run(combine.id), [17, -13]);
  assert.doesNotMatch(before, /implementationId|legacyDefault|capabilities|resultType/);
});

test('Combine Session disconnect, expression, fallback, atomic refusal and Undo/Redo survive reload', () => {
  const { doc, layer, combine } = scene();
  doc.schemaVersion = 26;
  combine.params.x.value = 11;
  combine.params.x.expression = 'value * 2';
  combine.params.y.value = -5;
  const value = node('Value'),
    vector = node('Vector');
  value.params.value.value = 17;
  layer.nodes.push(value, vector);
  connect(layer, value.id, combine.id, 'x');
  const session = new CommandSession(doc);
  const dispatch = (type, payload = {}) =>
    session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } });
  const run = () => evaluate(session.snapshot().layers[0]).run(combine.id);
  const before = serialize(session.snapshot());
  assert.deepEqual(run(), [17, -5]);
  dispatch('disconnect', { nodeId: combine.id, key: 'x' });
  const expression = serialize(session.snapshot());
  assert.deepEqual(run(), [22, -5]);
  assert.equal(session.historyView.past.length, 1);
  dispatch('undo');
  assert.equal(serialize(session.snapshot()), before);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(
    () => dispatch('connect', { sourceId: vector.id, nodeId: combine.id, key: 'x' }),
    /connect/i,
  );
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(JSON.stringify(session.historyView), history);
  assert.equal(serialize(session.snapshot()), before);
  dispatch('redo');
  assert.equal(serialize(session.snapshot()), expression);
  dispatch('set-parameter-expression', { nodeId: combine.id, key: 'x', expression: '' });
  assert.deepEqual(run(), [11, -5]);
  dispatch('set-parameter', { nodeId: combine.id, key: 'y', value: -21 });
  const after = serialize(session.snapshot());
  assert.deepEqual(run(), [11, -21]);
  dispatch('undo');
  assert.deepEqual(run(), [11, -5]);
  dispatch('redo');
  assert.equal(serialize(session.snapshot()), after);
  const loaded = deserialize(after);
  assert.equal(serialize(loaded), after);
  assert.deepEqual(evaluate(loaded.layers[0]).run(combine.id), [11, -21]);
  assert.equal(loaded.schemaVersion, 26);
});

test('Combine rejects nonfinite evaluated inputs instead of producing a default vector', () => {
  for (const key of ['x', 'y']) {
    const { layer, combine } = scene();
    combine.params[key].expression = '1 / 0';
    assert.throws(() => evaluate(layer).run(combine.id), /finite/);
  }
});
