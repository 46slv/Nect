import test from 'node:test';
import assert from 'node:assert/strict';
import { bypassContract, definitions, node, outputPorts, outputType } from '../src/core.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { dashPathDeclaration } from '../src/dash-path-declaration.js';
import { evaluateDashPath } from '../src/dash-path-binding.js';
import { createNodeRegistry, parseNodeDeclaration } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';

test('Dash Path declaration retains the existing factory and interface', () => {
  const parsed = parseNodeDeclaration(JSON.stringify(dashPathDeclaration)),
    resolved = createNodeRegistry(
      [parsed],
      new Map([['builtin.dash-path.v1', { evaluate: evaluateDashPath }]]),
      nectNodeHost,
    ).get('Dash Path'),
    n = node('Dash Path'),
    defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.deepEqual(builtinNodeRegistry.get(n.type).declaration, dashPathDeclaration);
  assert.equal(
    builtinManifest.find((row) => row.declaration.typeId === n.type).binding.evaluate,
    evaluateDashPath,
  );
  assert.equal(resolved.declaration.bypassInput, 'path');
  assert.ok(Object.isFrozen(resolved.declaration));
  assert.deepEqual(Object.keys(n.params), ['path', 'dashLength', 'gapLength', 'offset']);
  assert.deepEqual(n.params.dashLength, {
    type: 'Scalar',
    value: 12,
    expression: '',
    locked: false,
  });
  assert.deepEqual(n.params.gapLength, { type: 'Scalar', value: 6, expression: '', locked: false });
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  assert.equal(bypassContract(n).input, 'path');
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Smooth Path');
  assert.equal(defs[defs.indexOf(n.type) + 1], 'Simplify Path');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Auto Handles');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Transform');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, dashPathDeclaration.catalog);
});
