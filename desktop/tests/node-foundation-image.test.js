import test from 'node:test';
import assert from 'node:assert/strict';
import {
  bypassContract,
  CommandSession,
  connect,
  definitions,
  deserialize,
  document,
  evaluate,
  exportSVG,
  node,
  outputPorts,
  outputType,
  serialize,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { imageDeclaration } from '../src/image-node-declaration.js';
import { evaluateImage } from '../src/image-node-binding.js';
import { encodeRgbaPng, imageAssetFor } from '../src/image-keyer.js';
import { copyImageRgba } from '../src/image-luminance-field.js';
import { encodeImage } from '../src/images.js';

function asset(rgba) {
  return {
    name: 'fixture.png',
    mime: 'image/png',
    width: 2,
    height: 1,
    data: encodeImage(encodeRgbaPng(2, 1, new Uint8Array(rgba))),
  };
}

function fixture() {
  const doc = document(),
    layer = doc.layers[0],
    image = node('Image'),
    keyer = node('Keyer'),
    output = node('Image Output');
  doc.schemaVersion = 54;
  doc.imageAssets = {
    'image-fixture': asset([0, 255, 0, 255, 255, 0, 0, 128]),
  };
  image.asset = 'image-fixture';
  image.params.position.value = [100, 80];
  image.params.size.value = [40, 20];
  image.positionReference = 'center-middle';
  layer.nodes = [image, keyer, output];
  layer.output = output.id;
  connect(layer, image.id, keyer.id, 'path');
  connect(layer, keyer.id, output.id, 'path');
  return { doc, layer, image, keyer };
}

test('Image has its private registered interface and keeps definition order', () => {
  const registered = builtinNodeRegistry.get('Image'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Image'),
    current = node('Image');
  assert.ok(registered && manifest);
  assert.deepEqual(registered.declaration, imageDeclaration);
  assert.equal(manifest.binding.evaluate, evaluateImage);
  assert.equal(manifest.exposure, 'private');
  assert.equal(outputType(current), 'Image');
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'Image' }]);
  assert.equal(bypassContract(current).supported, false);
  assert.equal(current.asset, '');
  for (const parameter of imageDeclaration.parameters) {
    assert.equal(definitions.Image[parameter.id].type, parameter.type);
    assert.deepEqual(current.params[parameter.id].value, parameter.default);
  }
  assert.deepEqual(Object.keys(definitions).slice(0, 4), [
    'Image',
    'Image Output',
    'Keyer',
    'Mask',
  ]);
  assert.equal(
    nodeCatalog.some((entry) => entry.typeId === 'Image'),
    false,
  );
});

test('binding checks the asset and size before reading the position', () => {
  const values = { size: [40, 20], position: [100, 80] },
    assets = { 'image-fixture': { name: 'fixture' } },
    calls = [],
    context = {
      imageAssets: assets,
      imageAssetId() {
        calls.push('asset');
        return 'image-fixture';
      },
    },
    read = (key) => {
      calls.push(key);
      return values[key];
    };
  assert.deepEqual(evaluateImage(undefined, read, context), {
    kind: 'image',
    asset: 'image-fixture',
    position: [100, 80],
    size: [40, 20],
  });
  assert.deepEqual(calls, ['asset', 'size', 'position']);
  calls.length = 0;
  assert.throws(
    () => evaluateImage(undefined, read, { ...context, imageAssets: {} }),
    /Missing image asset/,
  );
  assert.deepEqual(calls, ['asset']);
  calls.length = 0;
  values.size = [0, 20];
  assert.throws(() => evaluateImage(undefined, read, context), /Image size must be positive/);
  assert.deepEqual(calls, ['asset', 'size']);
  assert.deepEqual(assets, { 'image-fixture': { name: 'fixture' } });
});

test('registered source keeps positioned image, Keyer pixels and asset cache invalidation', () => {
  const { doc, layer, image, keyer } = fixture(),
    saved = serialize(doc),
    first = evaluate(layer, {}, doc.imageAssets, doc),
    source = first.run(image.id),
    pixels = () => [
      ...copyImageRgba(
        imageAssetFor(evaluate(layer, {}, doc.imageAssets, doc).run(keyer.id), doc.imageAssets),
      ),
    ];
  assert.equal(first.error, null);
  assert.deepEqual(source, {
    kind: 'image',
    asset: 'image-fixture',
    position: [80, 70],
    size: [40, 20],
  });
  assert.deepEqual(pixels(), [0, 0, 0, 0, 255, 0, 0, 128]);
  const warm = evaluate(layer, {}, doc.imageAssets, doc);
  assert.deepEqual(warm.run(image.id), source);
  assert.ok(warm.stats.reused > 0);
  assert.equal(serialize(doc), saved);
  doc.imageAssets['image-fixture'] = asset([0, 0, 255, 255, 255, 0, 0, 128]);
  assert.deepEqual(pixels(), [0, 0, 255, 255, 255, 0, 0, 128]);
});

test('image size edits and failures remain atomic through history and save', () => {
  const { doc, layer, image } = fixture(),
    session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
    original = serialize(session.snapshot()),
    originalSvg = exportSVG(session.snapshot());
  send('set-parameter', { layerId: layer.id, nodeId: image.id, key: 'size', value: [60, 30] });
  const changed = serialize(session.snapshot());
  assert.notEqual(exportSVG(session.snapshot()), originalSvg);
  send('undo');
  assert.equal(serialize(session.snapshot()), original);
  assert.equal(exportSVG(session.snapshot()), originalSvg);
  send('redo');
  assert.equal(serialize(session.snapshot()), changed);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(
    () =>
      send('set-parameter', {
        layerId: layer.id,
        nodeId: image.id,
        key: 'size',
        value: ['bad', 1],
      }),
    /Expected Vector2/,
  );
  assert.equal(serialize(session.snapshot()), changed);
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(JSON.stringify(session.historyView), history);
  assert.equal(exportSVG(deserialize(changed)), exportSVG(session.snapshot()));
  send('set-parameter', { layerId: layer.id, nodeId: image.id, key: 'size', value: [0, 1] });
  const invalid = session.snapshot();
  assert.match(
    evaluate(invalid.layers[0], {}, invalid.imageAssets, invalid).error,
    /Image size must be positive/,
  );
  send('undo');
  assert.equal(serialize(session.snapshot()), changed);
});
