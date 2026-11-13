import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  bypassContract,
  CommandSession,
  connect,
  definitions,
  deserialize,
  enableRemapPoint,
  evaluate,
  exportSVG,
  node,
  outputPorts,
  outputType,
  serialize,
  socketCompatibleFor,
} from '../src/core.js';
import { encodeRgbaPng } from '../src/image-keyer.js';
import { encodeImage } from '../src/images.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { luminancePointsDeclaration } from '../src/luminance-points-node-declaration.js';
import { evaluateLuminancePoints } from '../src/luminance-points-node-binding.js';

test('Luminance to Points registration preserves defaults, input compatibility and placement', () => {
  const registered = builtinNodeRegistry.get('Luminance to Points'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Luminance to Points'),
    current = node('Luminance to Points'),
    types = Object.keys(definitions),
    catalog = nodeCatalog.map((entry) => entry.typeId);
  assert.ok(registered && manifest);
  assert.deepEqual(registered.declaration, luminancePointsDeclaration);
  assert.equal(manifest.binding.evaluate, evaluateLuminancePoints);
  assert.equal(outputType(current), 'Path');
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(bypassContract(current).supported, false);
  assert.equal(current.params.path.type, 'PaintedPath');
  assert.equal(current.params.pitch.value, 32);
  assert.deepEqual(current.params.gridOrigin.value, [0, 0]);
  assert.equal(current.params.maxRotation.value, 0);
  assert.equal(socketCompatibleFor(node('Image'), current, 'path'), true);
  assert.equal(types[types.indexOf('Luminance to Points') - 1], 'Luminance Instances');
  assert.equal(types[types.indexOf('Luminance to Points') + 1], 'Instance on Points');
  assert.equal(catalog[catalog.indexOf('Luminance to Points') - 1], 'Polar Transform');
  assert.equal(catalog[catalog.indexOf('Luminance to Points') + 1], 'Keyer');
  const { typeId, ...metadata } = nodeCatalog.find(
    (entry) => entry.typeId === 'Luminance to Points',
  );
  assert.equal(typeId, 'Luminance to Points');
  assert.deepEqual(metadata, luminancePointsDeclaration.catalog);
});

test('paint and image inputs keep point identity, source-change warnings and Session history', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    shape = node('Rectangle'),
    paint = node('Fill'),
    image = node('Image'),
    points = node('Luminance to Points'),
    motif = node('Curve'),
    instances = node('Instance on Points'),
    remap = node('Path Remap'),
    fill = node('Fill');
  doc.schemaVersion = 54;
  shape.params.center.value = [32, 32];
  shape.params.size.value = [64, 64];
  paint.params.color.value = { hex: '#000000', alpha: 1 };
  points.params.pitch.value = 16;
  image.asset = 'image-synthetic';
  image.params.position.value = [0, 0];
  image.params.size.value = [64, 64];
  doc.imageAssets = {
    'image-synthetic': {
      name: 'synthetic.png',
      mime: 'image/png',
      width: 1,
      height: 1,
      data: encodeImage(encodeRgbaPng(1, 1, new Uint8Array([0, 0, 0, 128]))),
    },
  };
  layer.nodes = [shape, paint, image, points, motif, instances, remap, fill, output];
  connect(layer, shape.id, paint.id, 'path');
  connect(layer, paint.id, points.id, 'path');
  connect(layer, motif.id, instances.id, 'path');
  connect(layer, points.id, instances.id, 'points');
  connect(layer, instances.id, remap.id, 'path');
  connect(layer, remap.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  const vector = evaluate(layer, {}, doc.imageAssets, doc),
    vectorPoints = vector.run(points.id),
    generated = vector.run(instances.id),
    target = generated.contours[0].points[0];
  assert.equal(vectorPoints.contours.length, 16);
  assert.equal(vectorPoints.contours[0].points[0].instanceTransform.scale, 1);
  enableRemapPoint(remap, generated, target.id).params.position.value = [2, 3];
  const session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
    before = serialize(session.snapshot());
  send('connect', { layerId: layer.id, nodeId: points.id, key: 'path', sourceId: image.id });
  const changed = serialize(session.snapshot()),
    snapshot = session.snapshot(),
    evaluation = evaluate(snapshot.layers[0], {}, snapshot.imageAssets, snapshot),
    imagePoints = evaluation.run(points.id);
  assert.equal(evaluation.error, null);
  assert.equal(imagePoints.contours.length, 16);
  assert.equal(imagePoints.contours[0].points[0].id, vectorPoints.contours[0].points[0].id);
  assert.ok(
    Math.abs(imagePoints.contours[0].points[0].instanceTransform.scale - 128 / 255) < 1e-12,
  );
  assert.match(evaluation.warnings.join(' '), /suspended/i);
  const reopened = deserialize(changed);
  assert.equal(exportSVG(reopened), exportSVG(snapshot));
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  send('redo');
  assert.equal(serialize(session.snapshot()), changed);
});
