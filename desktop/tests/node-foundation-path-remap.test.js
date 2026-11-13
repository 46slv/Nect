import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  bypassContract,
  connect,
  definitions,
  enableRemapPoint,
  evaluate,
  node,
  outputPorts,
  outputType,
  serialize,
  setNodeBypassed,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { pathRemapDeclaration } from '../src/path-remap-node-declaration.js';
import { evaluatePathRemap } from '../src/path-remap-node-binding.js';

test('Path Remap registration retains its interface, bypass and placement', () => {
  const registered = builtinNodeRegistry.get('Path Remap'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Path Remap'),
    current = node('Path Remap'),
    types = Object.keys(definitions),
    catalog = nodeCatalog.map((entry) => entry.typeId);
  assert.ok(registered && manifest);
  assert.deepEqual(registered.declaration, pathRemapDeclaration);
  assert.equal(manifest.binding.evaluate, evaluatePathRemap);
  assert.equal(outputType(current), 'Path');
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(bypassContract(current).input, 'path');
  assert.deepEqual(current.params.offset.value, [0, 0]);
  assert.deepEqual(current.edits, []);
  assert.equal(types[types.indexOf('Path Remap') - 1], 'Closed Path Offset');
  assert.equal(types[types.indexOf('Path Remap') + 1], 'Transform');
  assert.equal(catalog[catalog.indexOf('Path Remap') - 1], 'Transform');
  assert.equal(catalog[catalog.indexOf('Path Remap') + 1], 'Mirror');
  const { typeId, ...metadata } = nodeCatalog.find((entry) => entry.typeId === 'Path Remap');
  assert.equal(typeId, 'Path Remap');
  assert.deepEqual(metadata, pathRemapDeclaration.catalog);
});

test('suspended point edits and bypass do not evaluate inactive expressions', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    source = node('Imported Path'),
    remap = node('Path Remap'),
    fill = node('Fill');
  source.rawPath = 'M0 0L20 0L20 20Z';
  layer.nodes = [source, remap, fill, output];
  connect(layer, source.id, remap.id, 'path');
  connect(layer, remap.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  const sourcePath = evaluate(layer).run(source.id),
    first = sourcePath.contours[0].points[0].id,
    edit = enableRemapPoint(remap, sourcePath, first);
  edit.params.position.expression = '1 / 0; 0';
  source.rawPath = 'M0 0L25 0L20 20Z';
  const before = serialize(doc),
    suspended = evaluate(layer);
  assert.equal(suspended.error, null);
  assert.match(suspended.warnings.join(' '), /vertex edit suspended/);
  assert.equal(suspended.run(remap.id).remapSuspended, true);
  assert.equal(serialize(doc), before);

  source.rawPath = 'M0 0L20 0L20 20Z';
  assert.ok(evaluate(layer).error, 'the active invalid edit remains an error');
  remap.params.offset.expression = 'invalid(';
  setNodeBypassed(layer, remap.id, true);
  const bypassed = evaluate(layer);
  assert.equal(bypassed.error, null);
  assert.deepEqual(bypassed.run(remap.id), bypassed.run(source.id));
});
