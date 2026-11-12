import test from 'node:test';
import assert from 'node:assert/strict';
import { bypassContract, definitions, node, outputPorts, outputType } from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { circleDeclaration } from '../src/circle-node-declaration.js';
import { evaluateCircle } from '../src/circle-node-binding.js';
import { createNodeRegistry, parseNodeDeclaration } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';

test('Circle resolves through its trusted binding and retains the existing interface', () => {
  const registered = builtinNodeRegistry.get('Circle'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Circle'),
    parsed = parseNodeDeclaration(JSON.stringify(circleDeclaration)),
    resolved = createNodeRegistry(
      [parsed],
      new Map([['builtin.circle.v1', { evaluate: evaluateCircle }]]),
      nectNodeHost,
    ).get('Circle'),
    n = node('Circle');
  assert.equal(manifest.binding.evaluate, evaluateCircle);
  assert.deepEqual(registered.declaration, circleDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
  assert.equal(resolved.declaration.bypassInput, undefined);
  assert.deepEqual(Object.keys(n.params), ['center', 'radius']);
  assert.deepEqual(n.params.radius, { type: 'Scalar', value: 120, expression: '', locked: false });
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  assert.equal(bypassContract(n).supported, false);
  const defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Mask');
  assert.equal(defs[defs.indexOf(n.type) + 1], 'Ellipse');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Mask');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Ellipse');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, circleDeclaration.catalog);
});
