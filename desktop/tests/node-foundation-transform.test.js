import test from 'node:test';
import assert from 'node:assert/strict';
import {
  activeParameters,
  bypassContract,
  connect,
  definitions,
  document,
  node,
  outputPorts,
  outputType,
  serialize,
  transformExtensions,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { transformNodeDeclaration } from '../src/transform-node-declaration.js';
import { evaluateTransform } from '../src/transform-node-binding.js';

test('Transform registration preserves defaults, metadata and placement', () => {
  const current = node('Transform'),
    registered = builtinNodeRegistry.get('Transform'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Transform'),
    types = Object.keys(definitions),
    catalog = nodeCatalog.map((entry) => entry.typeId);
  assert.deepEqual(registered.declaration, transformNodeDeclaration);
  assert.equal(manifest.binding.evaluate, evaluateTransform);
  assert.equal(current.operation, 'subtract');
  assert.equal(outputType(current), 'Path');
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(bypassContract(current).input, 'path');
  assert.equal(types[types.indexOf('Transform') - 1], 'Path Remap');
  assert.equal(catalog[catalog.indexOf('Transform') - 1], 'Dash Path');
});

test('Transform shape follows Path and PaintedPath routing without copying authored values', () => {
  const doc = document(),
    layer = doc.layers[0],
    source = node('Rectangle'),
    fill = node('Fill'),
    transform = node('Transform'),
    registered = builtinNodeRegistry.get('Transform');
  layer.nodes = [source, fill, transform, layer.nodes[0]];
  transform.params.uniformScale = structuredClone(transformExtensions.uniformScale);
  transform.params.scaleWidth = structuredClone(transformExtensions.scaleWidth);
  connect(layer, source.id, transform.id, 'path');
  let before = serialize(doc),
    snapshot = registered.snapshot(transform);
  assert.equal(snapshot.mode, 'path');
  assert.equal(registered.interface(transform.operation, snapshot).resultType, 'Path');
  assert.ok(Object.isFrozen(snapshot) && Object.isFrozen(snapshot.slots));
  assert.ok(snapshot.slots.every((slot) => Object.keys(slot).join() === 'id'));
  assert.ok(!activeParameters(transform).some(([key]) => key === 'scaleWidth'));
  assert.equal(serialize(doc), before);
  connect(layer, source.id, fill.id, 'path');
  connect(layer, fill.id, transform.id, 'path');
  before = serialize(doc);
  snapshot = registered.snapshot(transform);
  assert.equal(snapshot.mode, 'painted');
  assert.equal(registered.interface(transform.operation, snapshot).resultType, 'PaintedPath');
  assert.equal(outputType(transform), 'PaintedPath');
  assert.equal(outputPorts(transform)[0].type, 'PaintedPath');
  assert.equal(bypassContract(transform).supported, true);
  assert.equal(serialize(doc), before);
});
