import test from 'node:test';
import assert from 'node:assert/strict';
import {
  activeParameters,
  blankDocument,
  bypassContract,
  connect,
  definitions,
  deserialize,
  evaluate,
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
import { createNodeRegistry } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';
import { nodeCatalog } from '../src/node-catalog.js';
import {
  halftoneDeclaration,
  orderedDitherDeclaration,
  errorDiffusionDeclaration,
  lineScreenDeclaration,
} from '../src/luminance-marks-node-declarations.js';
import {
  evaluateHalftone,
  evaluateOrderedDither,
  evaluateErrorDiffusion,
  evaluateLineScreen,
} from '../src/luminance-marks-node-binding.js';

const cases = [
  [halftoneDeclaration, evaluateHalftone, ['subtract', 'square', 'circle', 'polygon', 'star']],
  [orderedDitherDeclaration, evaluateOrderedDither, ['square', 'circle', 'polygon', 'star']],
  [errorDiffusionDeclaration, evaluateErrorDiffusion, ['square', 'circle', 'polygon', 'star']],
  [lineScreenDeclaration, evaluateLineScreen, ['horizontal', 'vertical']],
];

test('luminance mark registrations retain sockets, operations, bypass and catalog metadata', () => {
  const types = Object.keys(definitions),
    catalog = nodeCatalog.map((entry) => entry.typeId);
  for (const [declaration, binding, operations] of cases) {
    const type = declaration.typeId,
      registered = builtinNodeRegistry.get(type),
      manifest = builtinManifest.find((row) => row.declaration.typeId === type),
      current = node(type);
    assert.ok(registered && manifest);
    assert.deepEqual(registered.declaration, declaration);
    assert.equal(manifest.binding.evaluate, binding);
    assert.deepEqual(operationOptions[type], operations);
    assert.equal(current.operation, declaration.defaultOperation);
    assert.equal(outputType(current), 'PaintedPath');
    assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'PaintedPath' }]);
    assert.equal(bypassContract(current).input, 'path');
    assert.equal(current.params.pitch.value, 16);
    const { typeId, ...metadata } = nodeCatalog.find((entry) => entry.typeId === type);
    assert.equal(typeId, type);
    assert.deepEqual(metadata, declaration.catalog);
  }
  assert.equal(types[types.indexOf('Halftone') - 1], 'Angular Gradient Fill');
  assert.equal(types[types.indexOf('Line Screen') + 1], 'Stroke');
  assert.equal(catalog[catalog.indexOf('Halftone') - 1], 'Radial Gradient Fill');
  assert.equal(catalog[catalog.indexOf('Line Screen') - 1], 'Error Diffusion Dither Image');
});

test('supplemental input policy is validated and remains data-only', () => {
  const bindings = new Map([['builtin.halftone.v1', { evaluate: evaluateHalftone }]]);
  assert.throws(
    () =>
      createNodeRegistry(
        [{ ...halftoneDeclaration, preserveSupplementalInputs: 'yes' }],
        bindings,
        nectNodeHost,
      ),
    /supplemental input policy/,
  );
  const { operations, defaultOperation, ...withoutOperations } = halftoneDeclaration;
  assert.throws(
    () => createNodeRegistry([withoutOperations], bindings, nectNodeHost),
    /supplemental input policy/,
  );
});

test('authored point and edit dependencies remain active in every mark operation', () => {
  for (const [declaration, , operations] of cases) {
    const doc = blankDocument(),
      layer = doc.layers[0],
      output = layer.nodes[0],
      shape = node('Rectangle'),
      fill = node('Fill'),
      pointDriver = node('Vector'),
      editDriver = node('Vector'),
      effect = node(declaration.typeId),
      extra = point(1, 2);
    doc.schemaVersion = 54;
    extra.id = 'supplemental-point';
    effect.points.push(extra);
    effect.edits.push({
      pointId: 'supplemental-edit',
      params: {
        position: structuredClone(extra.position),
        handleA: structuredClone(extra.handleA),
        handleB: structuredClone(extra.handleB),
      },
    });
    layer.nodes = [shape, fill, pointDriver, editDriver, effect, output];
    connect(layer, shape.id, fill.id, 'path');
    connect(layer, fill.id, effect.id, 'path');
    connect(layer, pointDriver.id, effect.id, 'supplemental-point.position');
    connect(layer, editDriver.id, effect.id, 'remap.supplemental-edit.position');
    connect(layer, effect.id, output.id, 'path');
    assert.doesNotThrow(() => deserialize(serialize(doc)));
    for (const operation of operations) {
      setOperation(layer, effect.id, operation);
      const active = activeParameters(effect).map(([key]) => key),
        dependencies = nodeDependencies(effect, true).map((dependency) => dependency.source);
      assert.ok(active.includes('supplemental-point.position'));
      assert.ok(active.includes('remap.supplemental-edit.position'));
      assert.ok(dependencies.includes(pointDriver.id));
      assert.ok(dependencies.includes(editDriver.id));
    }
    layer.nodes = layer.nodes.filter((item) => item.id !== pointDriver.id);
    assert.equal(evaluate(layer, {}, doc.imageAssets, doc).error, 'Missing output socket');
  }
});
