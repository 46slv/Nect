import test from 'node:test';
import assert from 'node:assert/strict';
import {
  activeCompositionDocument,
  applyActiveCompositionDocument,
  COMPOSITION_SCHEMA_VERSION,
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
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
    editor: source.editor,
    layers: source.layers,
  });
  assert.deepEqual(migrated.fontAssets, source.fontAssets);
  assert.deepEqual(migrated.imageAssets, source.imageAssets);
  assert.equal('editor' in migrated, false);
  assert.deepEqual(migrated.compositions[0].editor, source.editor);
  assert.deepEqual(migrated.nodePresets, source.nodePresets);
  assert.equal(migrated.compositions[0].layers[0].nodes[0].asset, 'image-asset-1');
  assert.equal(migrated.compositions[0].layers[0].nodes[0].font, 'font-asset-1');
  assert.equal(migrated.compositions[0].layers[0].id, source.layers[0].id);
  assert.equal(migrated.compositions[0].layers[0].nodes[0].id, source.layers[0].nodes[0].id);
  assert.notStrictEqual(migrated.compositions[0].layers, source.layers, 'output is detached');
});

test('uses source dimensions and allocates a collision-free Composition ID', () => {
  const source = legacyDocument({ width: 1280, height: 720 });
  source.layers[0].id = 'legacy-composition-1';

  const migrated = migrateLegacyDocumentToCompositionProject(source);

  assert.deepEqual([migrated.compositions[0].width, migrated.compositions[0].height], [1280, 720]);
  assert.equal(migrated.compositions[0].id, 'legacy-composition-2');
  assert.equal(migrated.compositions[0].layers[0].id, 'legacy-composition-1');
});

test('rejects applying the legacy migration twice without mutating the migrated project', () => {
  const migrated = migrateLegacyDocumentToCompositionProject(legacyDocument()),
    before = JSON.stringify(migrated);

  assert.throws(() => migrateLegacyDocumentToCompositionProject(migrated), /more than once/);
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
  assert.throws(() => validateCompositionProject(ambiguousRoot), /legacy root document fields/);
});

function twoCompositionProject() {
  const first = migrateLegacyDocumentToCompositionProject(
      legacyDocument({ width: 1280, height: 720 }),
    ),
    secondSource = legacyDocument({ width: 320, height: 960 });
  secondSource.layers[0].id = 'layer-two';
  secondSource.layers[0].nodes[0].id = 'node-two';
  secondSource.layers[0].nodes[0].points[0].id = 'point-two';
  secondSource.layers[0].output = 'node-two';
  secondSource.editor.guides[0].id = 'guide-two';
  const second = migrateLegacyDocumentToCompositionProject(secondSource).compositions[0];
  second.id = 'composition-two';
  second.name = 'Tall format';
  first.compositions.push(second);
  return validateCompositionProject(first);
}

test('serializes and reopens multiple differently sized Compositions without loss', () => {
  const project = twoCompositionProject(),
    before = structuredClone(project),
    serialized = serializeCompositionProject(project),
    reopened = deserializeCompositionProject(serialized);

  assert.deepEqual(
    reopened.compositions.map(({ width, height }) => [width, height]),
    [
      [1280, 720],
      [320, 960],
    ],
  );
  assert.deepEqual(reopened, before);
  assert.deepEqual(project, before, 'serialization must not mutate the project');
});

test('keeps legacy Layer-graph references within one outer Composition', () => {
  const project = twoCompositionProject(),
    source = project.compositions[0].layers[0];
  source.nodes.push({
    id: 'reference-node',
    type: 'Composition Reference',
    compositionReference: { compositionId: source.id },
  });
  assert.strictEqual(validateCompositionProject(project), project);

  source.nodes[source.nodes.length - 1].compositionReference.compositionId =
    project.compositions[1].layers[0].id;
  assert.throws(() => validateCompositionProject(project), /cannot cross outer Compositions/);
});

test('null Composition References validate and round-trip without changing scope', () => {
  const project = twoCompositionProject(),
    source = project.compositions[0].layers[0];
  source.nodes.push({
    id: 'unbound-reference',
    type: 'Composition Reference',
    compositionReference: null,
  });
  const before = structuredClone(project);
  assert.strictEqual(validateCompositionProject(project), project);
  assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(project)), before);
  assert.deepEqual(project, before, 'validation and serialization are read-only');
});

test('only exact null is unbound; absent, malformed and missing targets still reject', () => {
  for (const target of [
    undefined,
    false,
    0,
    '',
    [],
    {},
    { compositionId: null },
    { compositionId: '' },
    { compositionId: [] },
    { compositionId: 'missing-layer' },
  ]) {
    const project = twoCompositionProject(),
      source = project.compositions[0].layers[0],
      reference = { id: 'invalid-reference', type: 'Composition Reference' };
    if (target !== undefined) reference.compositionReference = target;
    source.nodes.push(reference);
    const before = structuredClone(project);
    assert.throws(() => validateCompositionProject(project), /target is missing/);
    assert.deepEqual(project, before, 'rejection must not alter the candidate');
  }
});

test('legacy JSON is migrated once on read and new JSON is validated without remigration', () => {
  const legacy = legacyDocument({ width: 417, height: 263 }),
    migrated = deserializeCompositionProject(JSON.stringify(legacy)),
    compositionId = migrated.compositions[0].id,
    reopened = deserializeCompositionProject(serializeCompositionProject(migrated));

  assert.equal(migrated.compositions.length, 1);
  assert.deepEqual([migrated.compositions[0].width, migrated.compositions[0].height], [417, 263]);
  assert.equal(reopened.compositions.length, 1);
  assert.equal(reopened.compositions[0].id, compositionId);
  assert.deepEqual(reopened.compositions[0].editor, legacy.editor);
});

test('legacy JSON runs the core upgrader before outer Composition migration', () => {
  const legacy = legacyDocument(),
    sourceText = JSON.stringify(legacy);
  let calls = 0;
  const project = deserializeCompositionProject(sourceText, (text) => {
    calls++;
    const upgraded = JSON.parse(text);
    upgraded.schemaVersion = 27;
    upgraded.name = 'Core-upgraded project';
    return upgraded;
  });

  assert.equal(calls, 1);
  assert.equal(project.schemaVersion, 27);
  assert.equal(project.name, 'Core-upgraded project');
  assert.equal(project.compositions.length, 1);
  assert.equal(project.compositions[0].width, legacy.width);
});

test('active Document edits update one Composition and shared assets, preserving the other', () => {
  const project = twoCompositionProject(),
    before = structuredClone(project),
    activeId = project.compositions[0].id,
    inactiveBefore = structuredClone(project.compositions[1]),
    active = activeCompositionDocument(project, activeId);
  active.width = 1440;
  active.layers[0].name = 'Edited active layer';
  active.editor.guides[0].position = 25;
  active.imageAssets['image-asset-2'] = { name: 'new-image.png', data: 'new-image-bytes' };

  const updated = applyActiveCompositionDocument(project, activeId, active);

  assert.equal(updated.compositions[0].width, 1440);
  assert.equal(updated.compositions[0].height, 720);
  assert.equal(updated.compositions[0].layers[0].name, 'Edited active layer');
  assert.equal(updated.compositions[0].editor.guides[0].position, 25);
  assert.deepEqual(updated.compositions[1], inactiveBefore);
  assert.deepEqual(updated.imageAssets['image-asset-2'], {
    name: 'new-image.png',
    data: 'new-image-bytes',
  });
  assert.deepEqual(updated.fontAssets, before.fontAssets);
  assert.deepEqual(
    project,
    before,
    'project remains unchanged until the returned candidate is used',
  );
});

test('invalid active IDs and edited data fail without mutating the source project', () => {
  const project = twoCompositionProject(),
    before = JSON.stringify(project),
    validEdit = activeCompositionDocument(project, project.compositions[0].id);

  assert.throws(() => activeCompositionDocument(project, 'missing-composition'), /Unknown active/);
  assert.throws(
    () => applyActiveCompositionDocument(project, 'missing-composition', validEdit),
    /Unknown active/,
  );
  const invalidSize = structuredClone(validEdit);
  invalidSize.width = 0;
  assert.throws(() =>
    applyActiveCompositionDocument(project, project.compositions[0].id, invalidSize),
  );
  const duplicateId = structuredClone(validEdit);
  duplicateId.layers[0].nodes[0].id = project.compositions[1].layers[0].id;
  assert.throws(
    () => applyActiveCompositionDocument(project, project.compositions[0].id, duplicateId),
    /Duplicate ID/,
  );
  assert.equal(JSON.stringify(project), before);
});
