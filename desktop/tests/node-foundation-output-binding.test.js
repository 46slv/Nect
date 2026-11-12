import test from 'node:test';
import assert from 'node:assert/strict';
import { evaluateOutput } from '../src/output-node-binding.js';
import { outputDeclaration } from '../src/output-node-declaration.js';
import { createNodeRegistry, parseNodeDeclaration } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { bypassContract, definitions, node, outputPorts, outputType } from '../src/core.js';
import { nodeCatalog, nodePickerItems } from '../src/node-catalog.js';

test('Output declaration resolves the existing PaintedPath interface', () => {
  const parsed = parseNodeDeclaration(JSON.stringify(outputDeclaration)),
    resolved = createNodeRegistry(
      [parsed],
      new Map([['builtin.output.v1', { evaluate: evaluateOutput }]]),
      nectNodeHost,
    ).get('Output');
  assert.deepEqual(resolved.declaration.parameters, [
    { id: 'path', type: 'PaintedPath', default: null },
  ]);
  assert.equal(resolved.declaration.bypassInput, undefined);
  assert.equal(resolved.interface().resultType, 'PaintedPath');
  assert.ok(Object.isFrozen(resolved.declaration));
});

test('Output uses the private registered binding and retains its factory and placement', () => {
  const registered = builtinNodeRegistry.get('Output'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Output'),
    n = node('Output'),
    defs = Object.keys(definitions);
  assert.equal(manifest.exposure, 'private');
  assert.equal(manifest.binding.evaluate, evaluateOutput);
  assert.deepEqual(registered.declaration, outputDeclaration);
  assert.deepEqual(n.params.path, {
    type: 'PaintedPath',
    value: null,
    expression: '',
    locked: false,
  });
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'PaintedPath' }]);
  assert.equal(outputType(n), 'PaintedPath');
  assert.equal(bypassContract(n).supported, false);
  assert.equal(defs[defs.indexOf('Output') - 1], 'Stroke');
  assert.equal(defs[defs.indexOf('Output') + 1], 'Text');
  assert.equal(
    nodeCatalog.some((entry) => entry.typeId === 'Output'),
    false,
  );
  assert.equal(
    nodePickerItems(definitions).some((entry) => entry.id === 'Output'),
    false,
  );
});

test('Output binding treats intentional empty output separately from missing input', () => {
  const read = () => {
      throw Error('empty Output read its disconnected input');
    },
    context = {
      hasEmptyOutput: () => true,
      cloneEvaluated: () => {
        throw Error('empty Output attempted a clone');
      },
    };
  assert.deepEqual(evaluateOutput(undefined, read, context), { contours: [] });
  assert.throws(
    () =>
      evaluateOutput(undefined, () => null, {
        hasEmptyOutput: () => false,
        cloneEvaluated: context.cloneEvaluated,
      }),
    /Connect a PaintedPath input/,
  );
});

test('Output binding copies a connected evaluated value without changing it', () => {
  const source = { contours: [{ id: 'contour', closed: false, points: [] }] },
    reads = [];
  const result = evaluateOutput(
    undefined,
    (key) => {
      reads.push(key);
      return source;
    },
    {
      hasEmptyOutput: () => false,
      cloneEvaluated: structuredClone,
    },
  );
  assert.deepEqual(reads, ['path']);
  assert.deepEqual(result, source);
  assert.notEqual(result, source);
  assert.notEqual(result.contours, source.contours);
});
