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
import { createNodeRegistry } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { keyerDeclaration } from '../src/keyer-node-declaration.js';
import { evaluateKeyer } from '../src/keyer-node-binding.js';
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
  return { doc, layer, image, keyer };
}

test('production Keyer registration preserves defaults, Image ports, bypass and catalog', () => {
  const registered = builtinNodeRegistry.get('Keyer'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Keyer'),
    current = node('Keyer');
  assert.ok(registered && manifest);
  assert.deepEqual(registered.declaration, keyerDeclaration);
  assert.equal(manifest.binding.evaluate, evaluateKeyer);
  assert.equal(registered.interface().resultType, 'Image');
  assert.equal(outputType(current), 'Image');
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'Image' }]);
  assert.equal(bypassContract(current).input, 'path');
  for (const parameter of keyerDeclaration.parameters) {
    assert.equal(definitions.Keyer[parameter.id].type, parameter.type);
    assert.deepEqual(definitions.Keyer[parameter.id].value, parameter.default);
    assert.deepEqual(current.params[parameter.id].value, parameter.default);
  }
  const { typeId, ...metadata } = nodeCatalog.find((entry) => entry.typeId === 'Keyer');
  assert.equal(typeId, 'Keyer');
  assert.deepEqual(metadata, keyerDeclaration.catalog);
  const invalid = structuredClone(keyerDeclaration);
  invalid.parameters[0].default = { kind: 'image', asset: 'fixture' };
  assert.throws(
    () =>
      createNodeRegistry(
        [invalid],
        new Map([['builtin.keyer.v1', { evaluate: evaluateKeyer }]]),
        nectNodeHost,
      ),
    /default/,
  );
});

test('registered Keyer reads in order and keeps pixels and authored inputs intact', () => {
  const { doc, layer, image, keyer } = fixture(),
    before = serialize(doc),
    run = evaluate(layer, {}, doc.imageAssets, doc),
    source = run.run(image.id),
    expected = run.run(keyer.id),
    sourceBefore = structuredClone(source),
    values = {
      path: source,
      keyColor: keyer.params.keyColor.value,
      tolerance: keyer.params.tolerance.value,
      softness: keyer.params.softness.value,
    },
    reads = [];
  const actual = builtinNodeRegistry.get('Keyer').evaluate(
    keyer.operation,
    (key) => {
      reads.push(key);
      return values[key];
    },
    { imageAssets: doc.imageAssets },
  );
  assert.deepEqual(reads, ['path', 'keyColor', 'tolerance', 'softness']);
  assert.deepEqual(actual, expected);
  assert.deepEqual(source, sourceBefore);
  assert.deepEqual(
    [...copyImageRgba(imageAssetFor(actual, doc.imageAssets))],
    [0, 0, 0, 0, 255, 0, 0, 128, 255, 0, 0, 255, 0, 0, 0, 0],
  );
  assert.equal(serialize(doc), before);
  assert.throws(
    () =>
      evaluateKeyer('', (key) => ({ ...values, path: null })[key], {
        imageAssets: doc.imageAssets,
      }),
    /Connect an Image to Keyer/,
  );
  assert.throws(
    () =>
      evaluateKeyer('', (key) => ({ ...values, tolerance: 2 })[key], {
        imageAssets: doc.imageAssets,
      }),
    /range\/softness/,
  );
});

test('registered Keyer preserves bypass, Undo/Redo and native save/reopen', () => {
  const { doc, layer, keyer } = fixture(),
    session = new CommandSession(doc),
    dispatch = (type, payload = {}) =>
      session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } }),
    before = serialize(session.snapshot());
  dispatch('set-parameter', { nodeId: keyer.id, key: 'tolerance', value: 0.4 });
  const changed = serialize(session.snapshot());
  dispatch('undo');
  assert.equal(serialize(session.snapshot()), before);
  dispatch('redo');
  assert.equal(serialize(session.snapshot()), changed);
  dispatch('set-node-bypassed', { nodeId: keyer.id, bypassed: true });
  let snapshot = session.snapshot();
  assert.equal(
    evaluate(snapshot.layers[0], {}, snapshot.imageAssets, snapshot).path.imageEffects,
    undefined,
  );
  dispatch('undo');
  snapshot = deserialize(serialize(session.snapshot()));
  assert.equal(
    evaluate(snapshot.layers[0], {}, snapshot.imageAssets, snapshot).path.imageEffects.length,
    1,
  );
  assert.equal(serialize(snapshot), changed);
});
