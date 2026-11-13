import test from 'node:test';
import assert from 'node:assert/strict';
import {
  activeParameters,
  booleanPendingInputKey,
  bypassContract,
  connect,
  definitions,
  deserialize,
  document,
  node,
  nodeDependencies,
  operationOptions,
  outputPorts,
  outputType,
  point,
  serialize,
  setOperation,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { booleanNodeDeclaration } from '../src/boolean-node-declaration.js';
import { evaluateBoolean } from '../src/boolean-node-binding.js';

test('Boolean registration preserves its default interface and catalog placement', () => {
  const type = 'Boolean',
    current = node(type),
    registered = builtinNodeRegistry.get(type),
    manifest = builtinManifest.find((row) => row.declaration.typeId === type),
    types = Object.keys(definitions),
    catalog = nodeCatalog.map((entry) => entry.typeId);
  assert.deepEqual(registered.declaration, booleanNodeDeclaration);
  assert.equal(manifest.binding.evaluate, evaluateBoolean);
  assert.deepEqual(operationOptions[type], ['union', 'subtract', 'intersect', 'difference']);
  assert.equal(current.operation, 'subtract');
  assert.equal(current.booleanInputOrder, undefined);
  assert.equal(outputType(current), 'Path');
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(bypassContract(current).input, 'a');
  assert.equal(types[types.indexOf(type) - 1], 'Multi Path');
  assert.equal(catalog[catalog.indexOf(type) + 1], 'Join Geometry');
});

test('dynamic Boolean slots and supplemental dependencies stay active in every operation', () => {
  const doc = document(),
    layer = doc.layers[0],
    sources = Array.from({ length: 3 }, () => node('Rectangle')),
    driver = node('Vector'),
    effect = node('Boolean'),
    output = node('Output'),
    extra = point(1, 2);
  doc.schemaVersion = 54;
  extra.id = 'supplemental-point';
  effect.points.push(extra);
  layer.nodes = [...sources, driver, effect, output];
  layer.output = output.id;
  connect(layer, sources[0].id, effect.id, 'a');
  connect(layer, sources[1].id, effect.id, 'b');
  connect(layer, sources[2].id, effect.id, booleanPendingInputKey);
  connect(layer, driver.id, effect.id, 'supplemental-point.position');
  connect(layer, effect.id, output.id, 'path');
  assert.doesNotThrow(() => deserialize(serialize(doc)));
  const before = serialize(doc),
    registered = builtinNodeRegistry.get('Boolean'),
    shape = registered.snapshot(effect);
  assert.ok(Object.isFrozen(shape) && Object.isFrozen(shape.slots));
  assert.deepEqual(
    shape.slots.map((slot) => slot.id),
    effect.booleanInputOrder,
  );
  assert.equal(serialize(doc), before);
  for (const operation of operationOptions.Boolean) {
    setOperation(layer, effect.id, operation);
    const active = activeParameters(effect).map(([key]) => key),
      dependencies = nodeDependencies(effect, true).map((dependency) => dependency.source);
    assert.ok(active.includes(booleanPendingInputKey));
    assert.ok(active.includes('supplemental-point.position'));
    assert.ok(dependencies.includes(sources[2].id));
    assert.ok(dependencies.includes(driver.id));
  }
});
