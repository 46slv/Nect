import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  bypassContract,
  connect,
  definitions,
  evaluate,
  node,
  nodeDependencies,
  outputPorts,
  outputType,
  setReference,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { referenceDeclaration } from '../src/reference-node-declaration.js';
import { evaluateReference } from '../src/reference-node-binding.js';

test('Reference registration retains its Vector2 source interface and placement', () => {
  const registered = builtinNodeRegistry.get('Reference'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Reference'),
    current = node('Reference'),
    types = Object.keys(definitions),
    catalog = nodeCatalog.map((entry) => entry.typeId);
  assert.ok(registered && manifest);
  assert.deepEqual(registered.declaration, referenceDeclaration);
  assert.equal(manifest.binding.evaluate, evaluateReference);
  assert.deepEqual(current.params, {});
  assert.equal(current.reference, null);
  assert.equal(outputType(current), 'Vector2');
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'Vector2' }]);
  assert.equal(bypassContract(current).supported, false);
  assert.equal(types[types.indexOf('Reference') - 1], 'Null');
  assert.equal(types[types.indexOf('Reference') + 1], 'SVG');
  assert.equal(catalog[catalog.indexOf('Reference') - 1], 'Composition Reference');
  assert.equal(catalog[catalog.indexOf('Reference') + 1], 'Expression');
  const { typeId, ...metadata } = nodeCatalog.find((entry) => entry.typeId === 'Reference');
  assert.equal(typeId, 'Reference');
  assert.deepEqual(metadata, referenceDeclaration.catalog);
});

test('registered Reference returns an independent value and tracks the selected source', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    source = node('Vector'),
    reference = node('Reference'),
    circle = node('Circle'),
    fill = node('Fill');
  source.params.value.value = [120, 210];
  layer.nodes = [source, reference, circle, fill, output];
  connect(layer, reference.id, circle.id, 'center');
  connect(layer, circle.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  assert.throws(() => evaluate(layer).run(reference.id), /not selected/);
  setReference(layer, reference.id, { nodeId: source.id, portId: 'output' });
  assert.deepEqual(nodeDependencies(reference), [{ source: source.id, sourcePort: undefined }]);
  const current = evaluate(layer),
    original = current.run(source.id),
    result = current.run(reference.id);
  assert.deepEqual(result, [120, 210]);
  assert.notEqual(result, original);
  result[0] = -1;
  assert.deepEqual(original, [120, 210]);
  assert.deepEqual(source.params.value.value, [120, 210]);
  assert.deepEqual(evaluate(layer).run(reference.id), [120, 210]);
  source.params.value.value = [88, 99];
  assert.deepEqual(evaluate(layer).run(reference.id), [88, 99]);
  layer.nodes = layer.nodes.filter((item) => item.id !== source.id);
  assert.throws(() => evaluate(layer).run(reference.id), /Reference target is missing/);
});
