import test from 'node:test';
import assert from 'node:assert/strict';
import { bypassContract, definitions, node, outputPorts, outputType } from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { rectangleDeclaration } from '../src/rectangle-node-declaration.js';
import { evaluateRectangle } from '../src/rectangle-node-binding.js';
import { createNodeRegistry, parseNodeDeclaration } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';

test('Rectangle resolves through its trusted binding and retains the existing interface', () => {
  const registered = builtinNodeRegistry.get('Rectangle'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Rectangle'),
    parsed = parseNodeDeclaration(JSON.stringify(rectangleDeclaration)),
    resolved = createNodeRegistry(
      [parsed],
      new Map([['builtin.rectangle.v1', { evaluate: evaluateRectangle }]]),
      nectNodeHost,
    ).get('Rectangle'),
    n = node('Rectangle');
  assert.equal(manifest.binding.evaluate, evaluateRectangle);
  assert.deepEqual(registered.declaration, rectangleDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
  assert.equal(resolved.declaration.bypassInput, undefined);
  assert.deepEqual(Object.keys(n.params), ['center', 'size']);
  assert.deepEqual(n.params.size, {
    type: 'Vector2',
    value: [180, 220],
    expression: '',
    locked: false,
  });
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  assert.equal(bypassContract(n).supported, false);
  const defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Ellipse');
  assert.equal(defs[defs.indexOf(n.type) + 1], 'Polygon');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Ellipse');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Polygon');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, rectangleDeclaration.catalog);
});
