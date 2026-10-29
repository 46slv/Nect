import test from 'node:test';
import assert from 'node:assert/strict';
import {
  prepareProjectCompositionImport,
  prepareProjectCompositionsImport,
} from '../src/composition-project-import.js';
import { blankDocument, validate } from '../src/core.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  validateCompositionProject,
} from '../src/composition-project-model.js';

function project(name, compositions, assets = {}) {
  return validateCompositionProject({
    schemaVersion: 39,
    name,
    compositionSchemaVersion: 1,
    compositions,
    ...(assets.fontAssets ? { fontAssets: assets.fontAssets } : {}),
    ...(assets.imageAssets ? { imageAssets: assets.imageAssets } : {}),
  });
}

function destination() {
  return project(
    'Destination',
    [
      {
        id: 'comp-one',
        name: 'Current A',
        width: 200,
        height: 100,
        layers: [
          { id: 'layer-one', name: 'Existing A', nodes: [{ id: 'node-one', type: 'Output' }] },
        ],
      },
      {
        id: 'comp-two',
        name: 'Current B',
        width: 320,
        height: 960,
        layers: [
          { id: 'layer-two', name: 'Existing B', nodes: [{ id: 'node-two', type: 'Output' }] },
        ],
      },
    ],
    {
      fontAssets: { 'font-shared': { name: 'Destination font', data: 'destination-font' } },
      imageAssets: { 'image-shared': { name: 'Destination image', data: 'destination-image' } },
    },
  );
}

function source() {
  return project(
    'Source project',
    [
      {
        id: 'comp-one',
        name: 'Imported logo',
        width: 777,
        height: 333,
        layers: [
          {
            id: 'layer-one',
            name: 'Artwork',
            nodes: [
              {
                id: 'node-one',
                type: 'Composition Reference',
                compositionReference: { compositionId: 'layer-two' },
              },
              {
                id: 'font-node',
                type: 'Text',
                font: 'font-shared',
                characterPositions: { styles: [{ font: 'font-shared' }] },
              },
              { id: 'image-node', type: 'Image', asset: 'image-shared' },
            ],
          },
          {
            id: 'layer-two',
            name: 'Reference source',
            nodes: [{ id: 'source-node', type: 'Output' }],
          },
        ],
      },
      {
        id: 'comp-two',
        name: 'Imported portrait',
        width: 640,
        height: 480,
        layers: [{ id: 'source-extra-layer', name: 'Portrait stack', nodes: [] }],
      },
    ],
    {
      fontAssets: { 'font-shared': { name: 'Source font', data: 'source-font' } },
      imageAssets: { 'image-shared': { name: 'Source image', data: 'source-image' } },
    },
  );
}

test('imports all complete outer Compositions with dimensions, layers, references, and collision-safe assets', () => {
  const current = destination(),
    sourceProject = source(),
    unchanged = structuredClone(current);
  const { project: importedProject, compositionIds } = prepareProjectCompositionsImport(
    current,
    sourceProject,
  );

  assert.deepEqual(current, unchanged);
  assert.equal(importedProject.compositions.length, 4);
  assert.deepEqual(importedProject.compositions.slice(0, 2), current.compositions);
  assert.deepEqual(compositionIds, ['comp-one-import-1', 'comp-two-import-1']);
  const [logo, portrait] = importedProject.compositions.slice(2);
  assert.equal(logo.id, 'comp-one-import-1');
  assert.equal(logo.name, 'Imported logo');
  assert.deepEqual([logo.width, logo.height], [777, 333]);
  assert.deepEqual(
    logo.layers.map(({ name }) => name),
    ['Artwork', 'Reference source'],
  );
  assert.deepEqual(
    logo.layers.map(({ id }) => id),
    ['layer-one-import-1', 'layer-two-import-1'],
  );
  assert.equal(logo.layers[0].nodes[0].id, 'node-one-import-1');
  assert.equal(logo.layers[0].nodes[0].compositionReference.compositionId, logo.layers[1].id);
  assert.equal(portrait.id, 'comp-two-import-1');
  assert.equal(portrait.name, 'Imported portrait');
  assert.deepEqual([portrait.width, portrait.height], [640, 480]);
  assert.deepEqual(
    portrait.layers.map(({ name }) => name),
    ['Portrait stack'],
  );
  assert.equal(importedProject.fontAssets['font-shared'].data, 'destination-font');
  assert.equal(importedProject.fontAssets['font-shared-import-1'].data, 'source-font');
  assert.equal(importedProject.imageAssets['image-shared'].data, 'destination-image');
  assert.equal(importedProject.imageAssets['image-shared-import-1'].data, 'source-image');
  assert.equal(logo.layers[0].nodes[1].font, 'font-shared-import-1');
  assert.equal(logo.layers[0].nodes[1].characterPositions.styles[0].font, 'font-shared-import-1');
  assert.equal(logo.layers[0].nodes[2].asset, 'image-shared-import-1');
});

test('single-Composition import retains its explicit selection path', () => {
  const result = prepareProjectCompositionImport(destination(), source(), 'comp-two');
  assert.equal(result.project.compositions.length, 3);
  assert.equal(result.compositionId, 'comp-two-import-1');
  assert.equal(result.project.compositions[2].name, 'Imported portrait');
});

test('identical shared assets are reused without duplicate registry entries', () => {
  const current = destination(),
    sourceProject = source();
  sourceProject.fontAssets['font-shared'] = structuredClone(current.fontAssets['font-shared']);
  sourceProject.imageAssets['image-shared'] = structuredClone(current.imageAssets['image-shared']);
  const { project: importedProject } = prepareProjectCompositionsImport(current, sourceProject);

  assert.equal(Object.keys(importedProject.fontAssets).length, 1);
  assert.equal(Object.keys(importedProject.imageAssets).length, 1);
  assert.equal(importedProject.compositions[2].layers[0].nodes[1].font, 'font-shared');
  assert.equal(importedProject.compositions[2].layers[0].nodes[2].asset, 'image-shared');
});

test('invalid references or missing assets reject without mutating either project', () => {
  const current = destination(),
    currentBefore = structuredClone(current),
    sourceProject = source();
  delete sourceProject.fontAssets['font-shared'];

  assert.throws(
    () => prepareProjectCompositionsImport(current, sourceProject),
    /Missing font asset/,
  );
  assert.deepEqual(current, currentBefore);
  assert.equal(sourceProject.compositions.length, 2);
});

function groupedSource() {
  const document = blankDocument();
  document.schemaVersion = Math.max(document.schemaVersion, 44);
  const layer = document.layers[0];
  const memberId = layer.nodes[0].id;
  layer.nodeGroups = [{ id: 'group-src', name: memberId, nodeIds: [memberId] }];
  return migrateLegacyDocumentToCompositionProject(document);
}

test('Project Composition import preserves saved node-group members with and without ID collisions', () => {
  const sourceProject = groupedSource();
  const originalSource = serializeCompositionProject(sourceProject);
  const sourceLayer = sourceProject.compositions[0].layers[0];
  const sourceMemberId = sourceLayer.nodes[0].id;
  const sourceGroup = sourceLayer.nodeGroups[0];
  const unrelated = migrateLegacyDocumentToCompositionProject(blankDocument());
  unrelated.compositions[0].id = 'destination-composition';

  for (const [collision, current] of [
    [false, unrelated],
    [true, structuredClone(sourceProject)],
  ]) {
    const originalCurrent = serializeCompositionProject(current);
    const { project: importedProject, compositionId } = prepareProjectCompositionImport(
      current,
      sourceProject,
      sourceProject.compositions[0].id,
    );
    const imported = importedProject.compositions.find((item) => item.id === compositionId);
    const importedLayer = imported.layers[0];
    const importedMemberId = importedLayer.nodes[0].id;
    const importedGroup = importedLayer.nodeGroups[0];

    assert.equal(importedMemberId === sourceMemberId, !collision);
    assert.equal(importedGroup.id === sourceGroup.id, !collision);
    assert.deepEqual(importedGroup.nodeIds, [importedMemberId]);
    assert.equal(importedGroup.name, sourceGroup.name);
    assert.deepEqual(importedProject.compositions.slice(0, current.compositions.length), current.compositions);
    validate(activeCompositionDocument(importedProject, compositionId));

    const reopened = deserializeCompositionProject(serializeCompositionProject(importedProject));
    validate(activeCompositionDocument(reopened, compositionId));
    assert.equal(serializeCompositionProject(current), originalCurrent);
    assert.equal(serializeCompositionProject(sourceProject), originalSource);
  }
});
