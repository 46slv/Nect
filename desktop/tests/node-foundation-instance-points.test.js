import test from 'node:test';
import assert from 'node:assert/strict';
import { bypassContract, definitions, node, outputPorts, outputType } from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { instancePointsDeclaration } from '../src/instance-points-node-declaration.js';
import { evaluateInstancePoints } from '../src/instance-points-node-binding.js';

test('Instance on Points registration retains its sockets, bypass and catalog order', () => {
  const registered = builtinNodeRegistry.get('Instance on Points'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Instance on Points'),
    current = node('Instance on Points'),
    types = Object.keys(definitions),
    catalog = nodeCatalog.map((entry) => entry.typeId);
  assert.ok(registered && manifest);
  assert.deepEqual(registered.declaration, instancePointsDeclaration);
  assert.equal(manifest.binding.evaluate, evaluateInstancePoints);
  assert.equal(outputType(current), 'Path');
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(bypassContract(current).input, 'path');
  assert.deepEqual(Object.keys(current.params), ['path', 'points', 'alignTangent']);
  assert.equal(current.params.path.value, null);
  assert.equal(current.params.points.value, null);
  assert.equal(current.params.alignTangent.value, false);
  assert.equal(types[types.indexOf('Instance on Points') - 1], 'Luminance to Points');
  assert.equal(types[types.indexOf('Instance on Points') + 1], 'Value');
  assert.equal(catalog[catalog.indexOf('Instance on Points') - 1], 'Geometry Repeater');
  assert.equal(catalog[catalog.indexOf('Instance on Points') + 1], 'Luminance Instances');
  const { typeId, ...metadata } = nodeCatalog.find(
    (entry) => entry.typeId === 'Instance on Points',
  );
  assert.equal(typeId, 'Instance on Points');
  assert.deepEqual(metadata, instancePointsDeclaration.catalog);
});
