import test from 'node:test';
import assert from 'node:assert/strict';
import { bypassContract, definitions, node, outputPorts, outputType } from '../src/core.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { trimPathDeclaration } from '../src/trim-path-declaration.js';
import { evaluateTrimPath } from '../src/trim-path-binding.js';
import { createNodeRegistry, parseNodeDeclaration } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';

test('Trim Path declaration retains the existing factory and interface', () => {
  const parsed = parseNodeDeclaration(JSON.stringify(trimPathDeclaration)),
    resolved = createNodeRegistry(
      [parsed],
      new Map([['builtin.trim-path.v1', { evaluate: evaluateTrimPath }]]),
      nectNodeHost,
    ).get('Trim Path'),
    n = node('Trim Path'),
    defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(resolved.declaration.bypassInput, 'path');
  assert.ok(Object.isFrozen(resolved.declaration));
  assert.deepEqual(Object.keys(n.params), ['path', 'start', 'end']);
  assert.deepEqual(n.params.start, { type: 'Scalar', value: 0, expression: '', locked: false });
  assert.deepEqual(n.params.end, { type: 'Scalar', value: 1, expression: '', locked: false });
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  assert.equal(bypassContract(n).input, 'path');
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Expand Stroke');
  assert.equal(defs[defs.indexOf(n.type) + 1], 'Set Path Start');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Simplify Path');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Set Path Start');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, trimPathDeclaration.catalog);
});
