import test from 'node:test';
import assert from 'node:assert/strict';
import {
  activeParameters,
  bypassContract,
  definitions,
  node,
  operationOptions,
  outputPorts,
  outputType,
} from '../src/core.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { resamplePathDeclaration } from '../src/resample-path-declaration.js';
import { evaluateResamplePath } from '../src/resample-path-binding.js';
import { createNodeRegistry, parseNodeDeclaration } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';

test('Resample declaration retains current factory, operation interfaces and placement', () => {
  const parsed = parseNodeDeclaration(JSON.stringify(resamplePathDeclaration)),
    resolved = createNodeRegistry(
      [parsed],
      new Map([['builtin.resample-path.v1', { evaluate: evaluateResamplePath }]]),
      nectNodeHost,
    ).get('Resample Path'),
    n = node('Resample Path'),
    defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.deepEqual(builtinNodeRegistry.get(n.type).declaration, resamplePathDeclaration);
  assert.equal(
    builtinManifest.find((row) => row.declaration.typeId === n.type).binding.evaluate,
    evaluateResamplePath,
  );
  assert.ok(Object.isFrozen(resolved.declaration));
  assert.equal(resolved.declaration.bypassInput, 'path');
  assert.equal(n.operation, 'count');
  assert.deepEqual(operationOptions[n.type], ['count', 'spacing']);
  assert.deepEqual(resolved.interface('count').active, ['path', 'count']);
  assert.deepEqual(resolved.interface('spacing').active, ['path', 'spacing']);
  assert.deepEqual(
    activeParameters(n).map(([key]) => key),
    ['path', 'count'],
  );
  n.operation = 'spacing';
  assert.deepEqual(
    activeParameters(n).map(([key]) => key),
    ['path', 'spacing'],
  );
  assert.deepEqual(n.params.count, { type: 'Scalar', value: 32, expression: '', locked: false });
  assert.deepEqual(n.params.spacing, { type: 'Scalar', value: 24, expression: '', locked: false });
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  assert.equal(bypassContract(n).input, 'path');
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Roughen');
  assert.equal(defs[defs.indexOf(n.type) + 1], 'Corner Round');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Subdivide Path');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Auto Handles');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, resamplePathDeclaration.catalog);
});
