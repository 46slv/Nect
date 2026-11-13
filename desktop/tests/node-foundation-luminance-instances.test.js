import test from 'node:test';
import assert from 'node:assert/strict';
import {
  bypassContract,
  definitions,
  node,
  outputPorts,
  outputType,
  socketCompatibleFor,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { luminanceInstancesDeclaration } from '../src/luminance-instances-node-declaration.js';
import { evaluateLuminanceInstances } from '../src/luminance-instances-node-binding.js';

test('Luminance Instances registration retains its inputs, defaults, bypass and placement', () => {
  const registered = builtinNodeRegistry.get('Luminance Instances'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Luminance Instances'),
    current = node('Luminance Instances'),
    types = Object.keys(definitions),
    catalog = nodeCatalog.map((entry) => entry.typeId);
  assert.ok(registered && manifest);
  assert.deepEqual(registered.declaration, luminanceInstancesDeclaration);
  assert.equal(manifest.binding.evaluate, evaluateLuminanceInstances);
  assert.equal(outputType(current), 'Path');
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(bypassContract(current).input, 'motif');
  assert.equal(current.params.path.type, 'PaintedPath');
  assert.equal(current.params.motif.type, 'Path');
  assert.equal(current.params.pitch.value, 32);
  assert.equal(current.params.minScale.value, 0.2);
  assert.equal(current.params.maxRotation.value, 90);
  assert.equal(socketCompatibleFor(node('Image'), current, 'path'), true);
  assert.equal(types[types.indexOf('Luminance Instances') - 1], 'Geometry Repeater');
  assert.equal(types[types.indexOf('Luminance Instances') + 1], 'Luminance to Points');
  assert.equal(catalog[catalog.indexOf('Luminance Instances') - 1], 'Instance on Points');
  assert.equal(catalog[catalog.indexOf('Luminance Instances') + 1], 'Color');
  const { typeId, ...metadata } = nodeCatalog.find(
    (entry) => entry.typeId === 'Luminance Instances',
  );
  assert.equal(typeId, 'Luminance Instances');
  assert.deepEqual(metadata, luminanceInstancesDeclaration.catalog);
});
