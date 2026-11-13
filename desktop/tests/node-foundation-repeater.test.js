import test from 'node:test';
import assert from 'node:assert/strict';
import {
  activeParameters,
  bypassContract,
  connect,
  definitions,
  document,
  evaluate,
  node,
  operationOptions,
  outputPorts,
  outputType,
  setOperation,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { repeaterNodeDeclaration } from '../src/repeater-node-declaration.js';
import { evaluateRepeater } from '../src/repeater-node-binding.js';

test('repeater registration retains sockets, defaults, operations, bypass and placement', () => {
  const type = 'Geometry Repeater',
    current = node(type),
    registered = builtinNodeRegistry.get(type),
    manifest = builtinManifest.find((row) => row.declaration.typeId === type),
    types = Object.keys(definitions),
    catalog = nodeCatalog.map((entry) => entry.typeId);
  assert.deepEqual(registered.declaration, repeaterNodeDeclaration);
  assert.equal(manifest.binding.evaluate, evaluateRepeater);
  assert.deepEqual(operationOptions[type], ['linear', 'radial-fit', 'radial-step', 'ae-2d']);
  assert.equal(current.operation, 'linear');
  assert.equal(current.params.count.value, 6);
  assert.equal(outputType(current), 'Path');
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(bypassContract(current).input, 'path');
  assert.equal(types[types.indexOf(type) - 1], 'Mirror');
  assert.equal(catalog[catalog.indexOf(type) - 1], 'Join Geometry');
});

test('changing repeater operation activates only its relevant inputs', () => {
  const doc = document(),
    layer = doc.layers[0],
    shape = node('Rectangle'),
    repeater = node('Geometry Repeater'),
    fill = node('Fill'),
    output = node('Output');
  layer.nodes = [shape, repeater, fill, output];
  layer.output = output.id;
  connect(layer, shape.id, repeater.id, 'path');
  connect(layer, repeater.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  repeater.params.angle.expression = '1 / 0';
  assert.deepEqual(
    activeParameters(repeater).map(([key]) => key),
    ['path', 'count', 'step'],
  );
  assert.equal(evaluate(layer, {}, doc.imageAssets, doc).error, null);
  setOperation(layer, repeater.id, 'radial-step');
  assert.ok(activeParameters(repeater).some(([key]) => key === 'angle'));
  assert.ok(evaluate(layer, {}, doc.imageAssets, doc).error);
  setOperation(layer, repeater.id, 'linear');
  assert.equal(evaluate(layer, {}, doc.imageAssets, doc).error, null);
});
