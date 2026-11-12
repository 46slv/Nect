import test from 'node:test';
import assert from 'node:assert/strict';
import { bypassContract, definitions, node, outputPorts, outputType } from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { polygonDeclaration } from '../src/polygon-node-declaration.js';
import { evaluatePolygon } from '../src/polygon-node-binding.js';
import { createNodeRegistry, parseNodeDeclaration } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';

test('Polygon resolves through its trusted binding and retains the existing interface', () => {
  const registered = builtinNodeRegistry.get('Polygon'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Polygon'),
    parsed = parseNodeDeclaration(JSON.stringify(polygonDeclaration)),
    resolved = createNodeRegistry(
      [parsed],
      new Map([['builtin.polygon.v1', { evaluate: evaluatePolygon }]]),
      nectNodeHost,
    ).get('Polygon'),
    n = node('Polygon');
  assert.equal(manifest.binding.evaluate, evaluatePolygon);
  assert.deepEqual(registered.declaration, polygonDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
  assert.equal(resolved.declaration.bypassInput, undefined);
  assert.deepEqual(Object.keys(n.params), ['center', 'sides', 'radius', 'rotation']);
  assert.deepEqual(n.params.sides, { type: 'Scalar', value: 6, expression: '', locked: false });
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  assert.equal(bypassContract(n).supported, false);
  const defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Rectangle');
  assert.equal(defs[defs.indexOf(n.type) + 1], 'Star');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Rectangle');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Star');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, polygonDeclaration.catalog);
});
