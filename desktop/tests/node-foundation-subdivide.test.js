import test from 'node:test';
import assert from 'node:assert/strict';
import { bypassContract, definitions, node, outputPorts, outputType } from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { subdividePathDeclaration } from '../src/subdivide-path-declaration.js';
import { evaluateSubdividePath } from '../src/subdivide-path-binding.js';
import { createNodeRegistry, parseNodeDeclaration } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';

test('Subdivide Path resolves through its trusted binding and retains the existing interface', () => {
  const registered = builtinNodeRegistry.get('Subdivide Path'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Subdivide Path'),
    parsed = parseNodeDeclaration(JSON.stringify(subdividePathDeclaration)),
    resolved = createNodeRegistry(
      [parsed],
      new Map([['builtin.subdivide-path.v1', { evaluate: evaluateSubdividePath }]]),
      nectNodeHost,
    ).get('Subdivide Path'),
    n = node('Subdivide Path');
  assert.equal(manifest.binding.evaluate, evaluateSubdividePath);
  assert.deepEqual(registered.declaration, subdividePathDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
  assert.equal(resolved.declaration.bypassInput, 'path');
  assert.deepEqual(n.params.path, { type: 'Path', value: null, expression: '', locked: false });
  assert.deepEqual(n.params.segments, { type: 'Scalar', value: 2, expression: '', locked: false });
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  assert.equal(bypassContract(n).input, 'path');
  const defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Pucker & Bloat');
  assert.equal(defs[defs.indexOf(n.type) + 1], 'Roughen');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Reverse Path');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Resample Path');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, subdividePathDeclaration.catalog);
});
