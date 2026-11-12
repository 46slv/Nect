import test from 'node:test';
import assert from 'node:assert/strict';
import { bypassContract, definitions, node, outputPorts, outputType } from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { ellipseDeclaration } from '../src/ellipse-node-declaration.js';
import { evaluateEllipse } from '../src/ellipse-node-binding.js';
import { createNodeRegistry, parseNodeDeclaration } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';

test('Ellipse resolves through its trusted binding and retains the existing interface', () => {
  const registered = builtinNodeRegistry.get('Ellipse'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Ellipse'),
    parsed = parseNodeDeclaration(JSON.stringify(ellipseDeclaration)),
    resolved = createNodeRegistry(
      [parsed],
      new Map([['builtin.ellipse.v1', { evaluate: evaluateEllipse }]]),
      nectNodeHost,
    ).get('Ellipse'),
    n = node('Ellipse');
  assert.equal(manifest.binding.evaluate, evaluateEllipse);
  assert.deepEqual(registered.declaration, ellipseDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
  assert.equal(resolved.declaration.bypassInput, undefined);
  assert.deepEqual(Object.keys(n.params), ['center', 'size']);
  assert.deepEqual(n.params.size, {
    type: 'Vector2',
    value: [240, 160],
    expression: '',
    locked: false,
  });
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  assert.equal(bypassContract(n).supported, false);
  const defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Circle');
  assert.equal(defs[defs.indexOf(n.type) + 1], 'Rectangle');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Circle');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Rectangle');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, ellipseDeclaration.catalog);
});
