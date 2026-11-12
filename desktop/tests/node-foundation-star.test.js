import test from 'node:test';
import assert from 'node:assert/strict';
import { bypassContract, definitions, node, outputPorts, outputType } from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { starDeclaration } from '../src/star-node-declaration.js';
import { evaluateStar } from '../src/star-node-binding.js';
import { createNodeRegistry, parseNodeDeclaration } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';

test('Star resolves through its trusted binding and retains the existing interface', () => {
  const registered = builtinNodeRegistry.get('Star'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Star'),
    parsed = parseNodeDeclaration(JSON.stringify(starDeclaration)),
    resolved = createNodeRegistry(
      [parsed],
      new Map([['builtin.star.v1', { evaluate: evaluateStar }]]),
      nectNodeHost,
    ).get('Star'),
    n = node('Star');
  assert.equal(manifest.binding.evaluate, evaluateStar);
  assert.deepEqual(registered.declaration, starDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
  assert.equal(resolved.declaration.bypassInput, undefined);
  assert.deepEqual(Object.keys(n.params), [
    'center',
    'points',
    'outerRadius',
    'innerRadius',
    'rotation',
  ]);
  assert.deepEqual(n.params.points, { type: 'Scalar', value: 5, expression: '', locked: false });
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  assert.equal(bypassContract(n).supported, false);
  const defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Polygon');
  assert.equal(defs[defs.indexOf(n.type) + 1], 'Curve');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Polygon');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Curve');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, starDeclaration.catalog);
});
