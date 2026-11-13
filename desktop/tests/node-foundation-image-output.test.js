import test from 'node:test';
import assert from 'node:assert/strict';
import {
  CommandSession,
  bypassContract,
  connect,
  definitions,
  deserialize,
  document,
  evaluate,
  node,
  outputPorts,
  outputType,
  serialize,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog, nodePickerItems } from '../src/node-catalog.js';
import { imageOutputDeclaration } from '../src/image-output-declaration.js';
import { evaluateImageOutput } from '../src/image-output-binding.js';
import { encodeRgbaPng, imageAssetFor } from '../src/image-keyer.js';
import { copyImageRgba } from '../src/image-luminance-field.js';
import { encodeImage } from '../src/images.js';

function fixture() {
  const doc = document(),
    layer = doc.layers[0],
    image = node('Image'),
    keyer = node('Keyer'),
    output = node('Image Output');
  doc.schemaVersion = 54;
  doc.imageAssets = {
    'image-fixture': {
      name: 'fixture.png',
      mime: 'image/png',
      width: 2,
      height: 2,
      data: encodeImage(
        encodeRgbaPng(
          2,
          2,
          new Uint8Array([0, 255, 0, 255, 255, 0, 0, 128, 255, 0, 0, 255, 255, 0, 0, 0]),
        ),
      ),
    },
  };
  image.asset = 'image-fixture';
  layer.nodes = [image, keyer, output];
  layer.output = output.id;
  connect(layer, image.id, keyer.id, 'path');
  connect(layer, keyer.id, output.id, 'path');
  return { doc, layer, image, keyer, output };
}

test('Image Output retains its private registration, port, default and placement', () => {
  const registered = builtinNodeRegistry.get('Image Output'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Image Output'),
    current = node('Image Output'),
    types = Object.keys(definitions);
  assert.ok(registered && manifest);
  assert.equal(manifest.exposure, 'private');
  assert.equal(manifest.binding.evaluate, evaluateImageOutput);
  assert.deepEqual(registered.declaration, imageOutputDeclaration);
  assert.deepEqual(current.params.path, {
    type: 'Image',
    value: null,
    expression: '',
    locked: false,
  });
  assert.equal(outputType(current), 'Image');
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'Image' }]);
  assert.equal(bypassContract(current).supported, false);
  assert.equal(types[types.indexOf('Image Output') - 1], 'Image');
  assert.equal(types[types.indexOf('Image Output') + 1], 'Keyer');
  assert.equal(
    nodeCatalog.some((entry) => entry.typeId === 'Image Output'),
    false,
  );
  assert.equal(
    nodePickerItems(definitions).some((entry) => entry.id === 'Image Output'),
    false,
  );
});

test('Image Output clones its input and preserves existing rejection behavior', () => {
  const source = { kind: 'image', asset: 'image-fixture', position: [1, 2], size: [3, 4] },
    reads = [];
  const result = evaluateImageOutput(
    undefined,
    (key) => {
      reads.push(key);
      return source;
    },
    { cloneEvaluated: structuredClone },
  );
  assert.deepEqual(reads, ['path']);
  assert.deepEqual(result, source);
  assert.notEqual(result, source);
  assert.notEqual(result.position, source.position);
  assert.throws(
    () => evaluateImageOutput(undefined, () => null, { cloneEvaluated: structuredClone }),
    /Connect a Image input/,
  );
  assert.throws(
    () =>
      evaluateImageOutput(undefined, () => ({ kind: 'painted-group', children: [] }), {
        cloneEvaluated: structuredClone,
      }),
    /Edit paint in the source Composition/,
  );
});

test('Image Output preserves evaluated Keyer pixels, cache and authored image state', () => {
  const { doc, layer, keyer, output } = fixture(),
    authored = serialize(doc),
    first = evaluate(layer, {}, doc.imageAssets, doc),
    source = first.run(keyer.id),
    result = first.run(output.id);
  assert.equal(first.error, null);
  assert.deepEqual(result, source);
  assert.notEqual(result, source);
  assert.notEqual(result.imageEffects, source.imageEffects);
  assert.deepEqual(
    [...copyImageRgba(imageAssetFor(result, doc.imageAssets))],
    [0, 0, 0, 0, 255, 0, 0, 128, 255, 0, 0, 255, 0, 0, 0, 0],
  );
  const warm = evaluate(layer, {}, doc.imageAssets, doc);
  assert.deepEqual(warm.run(output.id), result);
  assert.ok(warm.stats.reused > 0);
  assert.equal(serialize(doc), authored);
});

test('Image Output connection, failed edit, Undo/Redo and save use the common Session', () => {
  const { doc, layer, output } = fixture(),
    rectangle = node('Rectangle');
  layer.nodes.push(rectangle);
  const session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: layer.id, nodeId: output.id, key: 'path', ...payload },
      }),
    connected = serialize(session.snapshot());
  send('disconnect');
  const disconnected = serialize(session.snapshot()),
    snapshot = session.snapshot();
  assert.match(
    evaluate(snapshot.layers[0], {}, snapshot.imageAssets, snapshot).error,
    /Connect a Image input/,
  );
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(() => send('connect', { sourceId: rectangle.id }), /Cannot connect|Incompatible/);
  assert.equal(serialize(session.snapshot()), disconnected);
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(JSON.stringify(session.historyView), history);
  send('undo');
  assert.equal(serialize(session.snapshot()), connected);
  send('redo');
  assert.equal(serialize(session.snapshot()), disconnected);
  send('undo');
  assert.equal(serialize(deserialize(serialize(session.snapshot()))), connected);
});
