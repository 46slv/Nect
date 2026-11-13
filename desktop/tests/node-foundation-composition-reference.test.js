import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  bypassContract,
  CommandSession,
  connect,
  definitions,
  deserialize,
  evaluate,
  exportSVG,
  node,
  outputPorts,
  outputType,
  pathBounds,
  serialize,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import {
  compositionReferenceDeclaration,
  svgReferenceDeclaration,
} from '../src/composition-reference-node-declarations.js';
import { evaluateCompositionReference } from '../src/composition-reference-node-binding.js';

test('Composition Reference and private SVG retain their output and placement', () => {
  const types = Object.keys(definitions),
    catalog = nodeCatalog.map((entry) => entry.typeId);
  for (const [type, declaration, exposure] of [
    ['Composition Reference', compositionReferenceDeclaration, 'public'],
    ['SVG', svgReferenceDeclaration, 'private'],
  ]) {
    const registered = builtinNodeRegistry.get(type),
      manifest = builtinManifest.find((row) => row.declaration.typeId === type),
      current = node(type);
    assert.ok(registered && manifest);
    assert.deepEqual(registered.declaration, declaration);
    assert.equal(manifest.binding.evaluate, evaluateCompositionReference);
    assert.equal(manifest.exposure, exposure);
    assert.equal(outputType(current), 'PaintedPath');
    assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'PaintedPath' }]);
    assert.equal(bypassContract(current).supported, false);
    assert.deepEqual(current.params.position.value, [0, 0]);
    assert.equal(current.compositionReference, null);
  }
  assert.equal(types[types.indexOf('SVG') - 1], 'Reference');
  assert.equal(types[types.indexOf('SVG') + 1], 'Composition Reference');
  assert.equal(types[types.indexOf('Composition Reference') + 1], 'Expression');
  assert.equal(catalog.includes('SVG'), false);
  assert.equal(catalog[catalog.indexOf('Composition Reference') - 1], 'Null');
  assert.equal(catalog[catalog.indexOf('Composition Reference') + 1], 'Reference');
  const { typeId, ...metadata } = nodeCatalog.find(
    (entry) => entry.typeId === 'Composition Reference',
  );
  assert.equal(typeId, 'Composition Reference');
  assert.deepEqual(metadata, compositionReferenceDeclaration.catalog);
});

test('both registered references follow their source through Session edits and history', () => {
  const doc = blankDocument(),
    source = doc.layers[0],
    target = blankDocument().layers[0],
    sourceOutput = source.nodes[0],
    targetOutput = target.nodes[0],
    shape = node('Rectangle'),
    fill = node('Fill'),
    reference = node('Composition Reference'),
    svg = node('SVG'),
    join = node('Join Geometry');
  doc.schemaVersion = 54;
  source.visible = false;
  shape.params.center.value = [50, 40];
  fill.params.color.value = { hex: '#bb2244', alpha: 0.5 };
  source.nodes = [shape, fill, sourceOutput];
  target.nodes = [reference, svg, join, targetOutput];
  doc.layers = [source, target];
  reference.compositionReference = { compositionId: source.id };
  svg.compositionReference = { compositionId: source.id };
  svg.svgAsset = {
    version: 1,
    mode: 'embedded',
    rootId: 'test',
    relativePath: 'synthetic.svg',
    sourceLocation: '',
    sha256: '0'.repeat(64),
    author: '',
    source: '',
    license: '',
  };
  reference.params.position.value = [10, 20];
  svg.params.position.value = [110, 5];
  connect(source, shape.id, fill.id, 'path');
  connect(source, fill.id, sourceOutput.id, 'path');
  connect(target, reference.id, join.id, 'a');
  connect(target, svg.id, join.id, 'b');
  connect(target, join.id, targetOutput.id, 'path');
  const session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
    before = serialize(session.snapshot()),
    beforeSvg = exportSVG(session.snapshot());
  send('set-parameter', { layerId: source.id, nodeId: shape.id, key: 'center', value: [70, 55] });
  const changed = serialize(session.snapshot()),
    snapshot = session.snapshot(),
    evaluation = evaluate(snapshot.layers[1], {}, snapshot.imageAssets, snapshot);
  assert.equal(evaluation.error, null);
  assert.equal(pathBounds(evaluation.run(reference.id))[0][1], 80);
  assert.equal(pathBounds(evaluation.run(svg.id))[0][1], 180);
  assert.notEqual(exportSVG(snapshot), beforeSvg);
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  assert.equal(exportSVG(session.snapshot()), beforeSvg);
  send('redo');
  assert.equal(serialize(session.snapshot()), changed);
  const reopened = deserialize(changed);
  assert.equal(exportSVG(reopened), exportSVG(session.snapshot()));
});
