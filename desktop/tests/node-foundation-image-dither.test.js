import test from 'node:test';
import assert from 'node:assert/strict';
import {
  activeParameters,
  blankDocument,
  bypassContract,
  connect,
  definitions,
  deserialize,
  node,
  nodeDependencies,
  operationOptions,
  outputPorts,
  outputType,
  point,
  serialize,
  setOperation,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import {
  orderedImageDitherDeclaration,
  errorDiffusionImageDitherDeclaration,
} from '../src/image-dither-node-declarations.js';
import {
  evaluateOrderedImageDither,
  evaluateErrorDiffusionImageDither,
} from '../src/image-dither-node-binding.js';

const cases = [
  [orderedImageDitherDeclaration, evaluateOrderedImageDither],
  [errorDiffusionImageDitherDeclaration, evaluateErrorDiffusionImageDither],
];
const operations = ['square', 'circle', 'polygon', 'star'];

test('image dither registrations retain their image sockets, operations, bypass and placement', () => {
  const types = Object.keys(definitions),
    catalog = nodeCatalog.map((entry) => entry.typeId);
  for (const [declaration, binding] of cases) {
    const type = declaration.typeId,
      registered = builtinNodeRegistry.get(type),
      manifest = builtinManifest.find((row) => row.declaration.typeId === type),
      current = node(type);
    assert.ok(registered && manifest);
    assert.deepEqual(registered.declaration, declaration);
    assert.equal(manifest.binding.evaluate, binding);
    assert.deepEqual(operationOptions[type], operations);
    assert.equal(current.operation, 'square');
    assert.equal(current.params.path.type, 'Image');
    assert.equal(outputType(current), 'Image');
    assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'Image' }]);
    assert.equal(bypassContract(current).input, 'path');
    const { typeId, ...metadata } = nodeCatalog.find((entry) => entry.typeId === type);
    assert.equal(typeId, type);
    assert.deepEqual(metadata, declaration.catalog);
  }
  assert.equal(types[types.indexOf('Ordered Dither Image') - 1], 'Imported Path');
  assert.equal(types[types.indexOf('Ordered Dither Image') + 1], 'Error Diffusion Dither Image');
  assert.equal(catalog[catalog.indexOf('Ordered Dither Image') - 1], 'Error Diffusion Dither');
  assert.equal(catalog[catalog.indexOf('Error Diffusion Dither Image') + 1], 'Line Screen');
});

test('authored supplemental dependencies stay active for each image dither operation', () => {
  for (const [declaration] of cases) {
    const doc = blankDocument(),
      layer = doc.layers[0],
      driver = node('Vector'),
      effect = node(declaration.typeId),
      extra = point(1, 2);
    doc.schemaVersion = 54;
    extra.id = 'supplemental-point';
    effect.points.push(extra);
    layer.nodes.push(driver, effect);
    connect(layer, driver.id, effect.id, 'supplemental-point.position');
    assert.doesNotThrow(() => deserialize(serialize(doc)));
    for (const operation of operations) {
      setOperation(layer, effect.id, operation);
      assert.ok(activeParameters(effect).some(([key]) => key === 'supplemental-point.position'));
      assert.ok(
        nodeDependencies(effect, true).some((dependency) => dependency.source === driver.id),
      );
    }
  }
});
