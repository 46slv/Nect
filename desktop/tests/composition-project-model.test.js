import test from 'node:test';
import assert from 'node:assert/strict';
import {
  COMPOSITION_SCHEMA_VERSION,
  migrateLegacyDocumentToCompositionProject,
  validateCompositionProject,
} from '../src/composition-project-model.js';

function legacyDocument({ width = 960, height = 540 } = {}) {
  return {
    schemaVersion: 39,
    name: 'Source project',
    width,
    height,
    editor: { guides: [{ id: 'guide-1', axis: 'x', position: 14 }] },
    fontAssets: { 'font-asset-1': { name: 'Example', data: 'font-bytes' } },
    imageAssets: { 'image-asset-1': { name: 'pixel.png', data: 'image-bytes' } },
    nodePresets: [{ id: 'preset-1', name: 'Saved graph' }],
    layers: [
      {
        id: 'layer-1',
        name: 'Artwork',
        visible: true,
        locked: false,
        fill: '#112233',
        nodes: [
          {
            id: 'node-1',
            type: 'Image',
            asset: 'image-asset-1',
            font: 'font-asset-1',
            points: [{ id: 'point-1', position: [2, 3] }],
          },
        ],
        output: 'node-1',
      },
    ],
  };
}

test('migrates a legacy frame into one outer Composition without changing its contents', () => {
  const source = legacyDocument(),
    before = JSON.stringify(source),
    migrated = migrateLegacyDocumentToCompositionProject(source);

  assert.equal(JSON.stringify(source), before, 'migration must not mutate its input');
  assert.equal(migrated.compositionSchemaVersion, COMPOSITION_SCHEMA_VERSION);
  assert.equal(migrated.schemaVersion, source.schemaVersion, 'node-content version stays separate');
  assert.equal(migrated.name, source.name);
  assert.equal('width' in migrated, false);
  assert.equal('height' in migrated, false);
  assert.equal('layers' in migrated, false);
  assert.equal(migrated.compositions.length, 1);
  assert.deepEqual(migrated.compositions[0], {
    id: 'legacy-composition-1',
    name: 'Composition 1',
    width: source.width,
    height: source.height,
    layers: source.layers,
  });
  assert.deepEqual(migrated.fontAssets, source.fontAssets);
  assert.deepEqual(migrated.imageAssets, source.imageAssets);
  assert.deepEqual(migrated.editor, source.editor);
  assert.deepEqual(migrated.nodePresets, source.nodePresets);
  assert.equal(migrated.compositions[0].layers[0].nodes[0].asset, 'image-asset-1');
  assert.equal(migrated.compositions[0].layers[0].nodes[0].font, 'font-asset-1');
  assert.equal(migrated.compositions[0].layers[0].id, source.layers[0].id);
  assert.equal(
    migrated.compositions[0].layers[0].nodes[0].id,
    source.layers[0].nodes[0].id,
  );
  assert.notStrictEqual(migrated.compositions[0].layers, source.layers, 'output is detached');
});

test('uses source dimensions and allocates a collision-free Composition ID', () => {
  const source = legacyDocument({ width: 1280, height: 720 });
  source.layers[0].id = 'legacy-composition-1';

  const migrated = migrateLegacyDocumentToCompositionProject(source);

  assert.deepEqual(
    [migrated.compositions[0].width, migrated.compositions[0].height],
    [1280, 720],
  );
  assert.equal(migrated.compositions[0].id, 'legacy-composition-2');
  assert.equal(migrated.compositions[0].layers[0].id, 'legacy-composition-1');
});

test('rejects applying the legacy migration twice without mutating the migrated project', () => {
  const migrated = migrateLegacyDocumentToCompositionProject(legacyDocument()),
    before = JSON.stringify(migrated);

  assert.throws(
    () => migrateLegacyDocumentToCompositionProject(migrated),
    /more than once/,
  );
  assert.equal(JSON.stringify(migrated), before);
  assert.equal(migrated.compositions.length, 1);
});

test('rejects missing, non-finite, non-positive and over-limit legacy dimensions', () => {
  for (const [width, height] of [
    [undefined, 540],
    [960, undefined],
    [0, 540],
    [-1, 540],
    [960, 0],
    [960, -1],
    [Number.NaN, 540],
    [960, Number.POSITIVE_INFINITY],
    [100001, 540],
    [960, 100001],
  ]) {
    const source = legacyDocument();
    source.width = width;
    source.height = height;
    assert.throws(() => migrateLegacyDocumentToCompositionProject(source));
  }
});

test('rejects duplicate entity IDs in the legacy project and new envelope', () => {
  const duplicateLegacy = legacyDocument();
  duplicateLegacy.layers[0].nodes[0].id = duplicateLegacy.layers[0].id;
  assert.throws(() => migrateLegacyDocumentToCompositionProject(duplicateLegacy), /Duplicate ID/);

  const duplicateNew = migrateLegacyDocumentToCompositionProject(legacyDocument());
  duplicateNew.compositions[0].layers[0].nodes[0].id = duplicateNew.compositions[0].id;
  assert.throws(() => validateCompositionProject(duplicateNew), /Duplicate ID/);
});

test('validates Composition dimensions and rejects ambiguous legacy root fields', () => {
  const valid = migrateLegacyDocumentToCompositionProject(legacyDocument());
  assert.strictEqual(validateCompositionProject(valid), valid);

  const invalidSize = structuredClone(valid);
  invalidSize.compositions[0].height = 100001;
  assert.throws(() => validateCompositionProject(invalidSize), /Invalid Composition height/);

  const ambiguousRoot = structuredClone(valid);
  ambiguousRoot.width = 960;
  assert.throws(() => validateCompositionProject(ambiguousRoot), /legacy root frame fields/);
});
