import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { blankDocument, node } from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { imageData } from './fixtures/artwork-deletion.js';

function project() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    driver = node('Value'),
    curve = node('Curve');
  doc.schemaVersion = 65;
  layer.id = 'shared-layer';
  layer.nodes[0].id = 'shared-output';
  layer.output = 'shared-output';
  driver.id = 'driver';
  curve.id = 'curve';
  curve.points[0].id = 'anchor';
  layer.nodes.push(driver, curve);
  const p = migrateLegacyDocumentToCompositionProject(doc);
  p.compositions[0].id = 'shared-composition';
  return p;
}

const send = (owner, type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });

test('opaque metadata stays literal through entity remapping and aliased parameter descriptors', () => {
  const source = project(),
    destination = project();
  const composition = source.compositions[0],
    layer = composition.layers[0];
  const driver = layer.nodes.find((n) => n.id === 'driver');
  const curve = layer.nodes.find((n) => n.id === 'curve');
  const sink = node('Value');
  sink.id = 'sink';
  sink.params.value.source = 'driver';
  sink.metadata = sink.params.value;
  const metadata = { id: 'driver', source: 'driver', layerId: 'shared-layer', pointId: 'anchor' };
  composition.metadata = structuredClone(metadata);
  layer.metadata = structuredClone(metadata);
  driver.metadata = structuredClone(metadata);
  curve.points[0].metadata = structuredClone(metadata);
  sink.params.value.metadata = structuredClone(metadata);
  layer.nodes.push(sink);
  destination.compositions[0].metadata = { id: 'driver', source: 'driver' };
  const expectedSource = structuredClone(source);
  const owner = createCompositionSessionOwner(structuredClone(destination));
  const originalDestination = owner.projectSnapshot();
  const before = serializeCompositionProject(owner.projectSnapshot());
  const result = send(owner, 'import-project-compositions', { sourceProject: source });
  const copied = owner
    .projectSnapshot()
    .compositions.find((c) => c.id === result.importedCompositionIds[0]);
  const copiedLayer = copied.layers[0],
    copiedDriver = copiedLayer.nodes.find((n) => n.id === 'driver-import-1'),
    copiedCurve = copiedLayer.nodes.find((n) => n.id === 'curve-import-1'),
    copiedSink = copiedLayer.nodes.find((n) => n.id === 'sink');
  assert.equal(copied.id, 'shared-composition-import-1');
  assert.equal(copiedLayer.id, 'shared-layer-import-1');
  assert.equal(copiedCurve.points[0].id, 'anchor-import-1');
  assert.equal(copiedSink.params.value.source, 'driver-import-1');
  for (const item of [
    copied.metadata,
    copiedLayer.metadata,
    copiedDriver.metadata,
    copiedCurve.points[0].metadata,
    copiedSink.params.value.metadata,
  ])
    assert.deepEqual(item, metadata);
  assert.deepEqual(
    copiedSink.metadata,
    expectedSource.compositions[0].layers[0].nodes.find((n) => n.id === 'sink').metadata,
  );
  assert.deepEqual(source, expectedSource);
  assert.deepEqual(owner.projectSnapshot().compositions[0], originalDestination.compositions[0]);
  assert.deepEqual(destination, originalDestination);
  const after = serializeCompositionProject(owner.projectSnapshot());
  send(owner, 'undo');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
  send(owner, 'redo');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), after);
  const reopened = createCompositionSessionOwner(deserializeCompositionProject(after));
  assert.equal(serializeCompositionProject(reopened.projectSnapshot()), after);
});

test('font and image references in metadata are opaque, while owned asset references still remap', () => {
  const source = project(),
    destination = project();
  const fontData = fs.readFileSync(
    new URL('../public/fonts/DejaVuSans.ttf', import.meta.url),
    'base64',
  );
  const font = (name) => ({ name, fileName: 'DejaVuSans.ttf', data: fontData });
  const image = (name) => ({ name, mime: 'image/png', width: 1, height: 1, data: imageData });
  source.fontAssets = { 'font-collision': font('Source font') };
  destination.fontAssets = { 'font-collision': font('Destination font') };
  source.imageAssets = { 'image-collision': image('source.png') };
  destination.imageAssets = { 'image-collision': image('destination.png') };
  const text = node('Text'),
    imageNode = node('Image');
  text.id = 'source-text';
  text.font = 'font-collision';
  text.metadata = {
    font: 'font-collision',
    asset: 'image-collision',
    nested: { font: 'font-missing', asset: 'image-missing' },
  };
  imageNode.id = 'source-image';
  imageNode.asset = 'image-collision';
  source.compositions[0].layers[0].nodes.push(text, imageNode);
  const sourceBefore = structuredClone(source),
    owner = createCompositionSessionOwner(structuredClone(destination));
  const originalDestination = owner.projectSnapshot();
  const result = send(owner, 'import-project-compositions', { sourceProject: source });
  const imported = owner
    .projectSnapshot()
    .compositions.find((c) => c.id === result.importedCompositionIds[0]);
  const copiedText = imported.layers[0].nodes.find((n) => n.id === 'source-text');
  const copiedImage = imported.layers[0].nodes.find((n) => n.id === 'source-image');
  assert.equal(copiedText.font, 'font-collision-import-1');
  assert.equal(copiedImage.asset, 'image-collision-import-1');
  assert.deepEqual(copiedText.metadata, text.metadata);
  assert.deepEqual(source, sourceBefore);
  const snapshot = owner.projectSnapshot();
  assert.deepEqual(snapshot.compositions[0], originalDestination.compositions[0]);
  assert.deepEqual(
    snapshot.fontAssets['font-collision'],
    originalDestination.fontAssets['font-collision'],
  );
  assert.deepEqual(
    snapshot.imageAssets['image-collision'],
    originalDestination.imageAssets['image-collision'],
  );
  assert.deepEqual(destination, originalDestination);
});

test('metadata-only IDs do not consume entity IDs', () => {
  const source = project(),
    destination = project();
  const targetDriver = destination.compositions[0].layers[0].nodes.find((n) => n.id === 'driver');
  targetDriver.id = 'other-driver';
  destination.compositions[0].metadata = { id: 'driver' };
  const owner = createCompositionSessionOwner(destination);
  const result = send(owner, 'import-project-compositions', { sourceProject: source });
  const imported = owner
    .projectSnapshot()
    .compositions.find((c) => c.id === result.importedCompositionIds[0]);
  assert.ok(imported.layers[0].nodes.some((n) => n.id === 'driver'));
});
