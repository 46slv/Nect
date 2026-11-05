import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  outputPorts,
  outputType,
  evaluate,
  connect,
  serialize,
  deserialize,
  definitions,
  activeParameters,
} from '../src/core.js';
import { SEPARATE_VECTOR } from '../src/builtin-node-registry.js';
import { nodePickerItems } from '../src/node-catalog.js';
import { createNodeRegistry } from '../src/node-foundation.js';
import { separateVectorDeclaration } from '../src/separate-vector-declaration.js';
import { nectNodeHost } from '../src/nect-node-host.js';

test('Separate Vector uses registered factory, named ports, runtime and authored representation', () => {
  const doc = document(),
    layer = doc.layers[0],
    vector = node('Vector'),
    separate = node('Separate Vector'),
    math = node('Math');
  layer.nodes.push(vector, separate, math);
  assert.equal(separate.operation, 'subtract');
  assert.deepEqual(separate.params.vector.value, [0, 0]);
  assert.deepEqual(
    activeParameters(separate).map(([id]) => id),
    ['vector'],
  );
  assert.deepEqual(
    outputPorts(separate),
    SEPARATE_VECTOR.declaration.outputs.map(({ id, type, label }) => ({ id, type, label })),
  );
  assert.deepEqual(
    outputPorts({ ...separate, paintMode: true }).map((p) => p.type),
    ['Scalar', 'Scalar'],
  );
  assert.equal(outputType(separate), undefined);
  assert.equal(outputType(separate, 'z'), undefined);
  assert.equal(
    nodePickerItems(definitions).find((item) => item.typeId === separate.type).labelJa,
    SEPARATE_VECTOR.declaration.catalog.labelJa,
  );
  vector.params.value.value = [12, -7];
  connect(layer, vector.id, separate.id, 'vector');
  connect(layer, separate.id, math.id, 'a', false, 'x');
  const engine = evaluate(layer);
  assert.equal(engine.run(separate.id, 'x'), 12);
  assert.equal(engine.run(separate.id, 'y'), -7);
  assert.equal(engine.run(math.id), 12);
  assert.throws(() => engine.run(separate.id), /socket/);
  assert.throws(() => engine.run(separate.id, 'z'), /socket/);
  const before = serialize(doc);
  assert.throws(() => connect(layer, separate.id, math.id, 'a', false, 'z'), /connect/i);
  assert.equal(serialize(doc), before);
  assert.equal(serialize(deserialize(before)), before);
  assert.doesNotMatch(before, /implementationId|legacyDefault|capabilities/);
});

test('Separate Vector declaration rejects invalid adapters, port and vector defaults', () => {
  const binding = new Map([
    ['builtin.separate-vector.v1', { evaluate: (_operation, read) => read('vector') }],
  ]);
  const bad = structuredClone(separateVectorDeclaration);
  bad.parameters[0].default = [1, 'bad'];
  assert.throws(() => createNodeRegistry([bad], binding, nectNodeHost), /default/);
  const duplicate = structuredClone(separateVectorDeclaration);
  duplicate.outputs[1].id = 'x';
  assert.throws(() => createNodeRegistry([duplicate], binding, nectNodeHost), /Duplicate/);
  assert.throws(() => SEPARATE_VECTOR.project('unknown', [1, 2]), /output/);
  assert.throws(
    () => createNodeRegistry([separateVectorDeclaration], binding, nectNodeHost),
    /projection/,
  );
});
