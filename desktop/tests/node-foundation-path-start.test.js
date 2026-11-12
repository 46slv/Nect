import test from 'node:test';
import assert from 'node:assert/strict';
import { bypassContract, definitions, node, outputPorts, outputType } from '../src/core.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { setPathStartDeclaration } from '../src/set-path-start-declaration.js';
import { evaluateSetPathStart } from '../src/set-path-start-binding.js';
import { createNodeRegistry, parseNodeDeclaration } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';

test('Set Path Start declaration retains the existing factory and interface', () => {
  const parsed = parseNodeDeclaration(JSON.stringify(setPathStartDeclaration)),
    resolved = createNodeRegistry(
      [parsed],
      new Map([['builtin.set-path-start.v1', { evaluate: evaluateSetPathStart }]]),
      nectNodeHost,
    ).get('Set Path Start'),
    n = node('Set Path Start'),
    defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.deepEqual(builtinNodeRegistry.get(n.type).declaration, setPathStartDeclaration);
  assert.equal(
    builtinManifest.find((row) => row.declaration.typeId === n.type).binding.evaluate,
    evaluateSetPathStart,
  );
  assert.equal(resolved.declaration.bypassInput, 'path');
  assert.ok(Object.isFrozen(resolved.declaration));
  assert.deepEqual(Object.keys(n.params), ['path', 'start']);
  assert.deepEqual(n.params.start, { type: 'Scalar', value: 0, expression: '', locked: false });
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  assert.equal(bypassContract(n).input, 'path');
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Trim Path');
  assert.equal(defs[defs.indexOf(n.type) + 1], 'Set Closure');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Trim Path');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Set Closure');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, setPathStartDeclaration.catalog);
});
