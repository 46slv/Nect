import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, blankLayer, node, exportSVG } from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { Window } from 'happy-dom';
import { readSVGSource } from '../src/svg-tree.js';

function fixture() {
  const doc = blankDocument();
  doc.layers = ['Back', 'Middle', 'Front', 'Top'].map((name, index) => {
    const layer = blankLayer(name),
      rectangle = node('Rectangle'),
      fill = node('Fill');
    rectangle.params.center.value = [100, 100];
    rectangle.params.size.value = [80, 80];
    fill.params.path.source = rectangle.id;
    fill.params.color.value = {
      hex: ['#ff0000', '#00ff00', '#0000ff', '#ffff00'][index],
      alpha: 0.5,
    };
    layer.nodes.unshift(rectangle, fill);
    layer.nodes.find((item) => item.type === 'Output').params.path.source = fill.id;
    return layer;
  });
  const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc));
  const send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
  return { owner, send, ids: doc.layers.map(({ id }) => id) };
}

test('Folder organization keeps painted Layers and references unchanged through Undo and reopen', () => {
  const { owner, send, ids } = fixture(),
    before = owner.snapshot(),
    beforeSvg = exportSVG(before),
    beforeHistory = owner.historyView.past.length;
  const { folderId } = send('create-layer-folder', { name: 'Shapes', layerIds: [ids[2], ids[1]] });
  const grouped = owner.snapshot();
  assert.deepEqual(grouped.layers, before.layers);
  assert.equal(exportSVG(grouped), beforeSvg);
  assert.deepEqual(grouped.editor.layerFolders[0], {
    id: folderId,
    name: 'Shapes',
    members: [{ layerId: ids[1] }, { layerId: ids[2] }],
  });
  assert.equal(owner.historyView.past.length, beforeHistory + 1);
  const project = owner.projectSnapshot();
  assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(project)), project);
  send('undo');
  assert.deepEqual(owner.snapshot(), before);
  send('redo');
  assert.deepEqual(owner.snapshot(), grouped);
  send('rename-layer-folder', { folderId, name: 'Foreground' });
  send('add-layers-to-folder', { folderId, layerIds: [ids[3]] });
  send('remove-layers-from-folder', { folderId, layerIds: [ids[1]] });
  assert.deepEqual(owner.snapshot().layers, before.layers);
  assert.equal(exportSVG(owner.snapshot()), beforeSvg);
  assert.deepEqual(owner.snapshot().editor.layerFolders[0].members, [
    { layerId: ids[2] },
    { layerId: ids[3] },
  ]);
});

test('Folder constraints reject non-contiguous operations without changing History or Redo', () => {
  const { owner, send, ids } = fixture();
  const { folderId } = send('create-layer-folder', { name: 'Pair', layerIds: [ids[1], ids[2]] });
  send('rename-layer-folder', { folderId, name: 'Changed' });
  send('undo');
  const before = owner.projectSnapshot(),
    state = owner.state,
    history = owner.historyView.future.length;
  assert.throws(
    () => send('add-layers-to-folder', { folderId, layerIds: [ids[0], ids[3], ids[1]] }),
    /Folder|Layer/,
  );
  assert.throws(
    () => send('create-layer-folder', { name: 'Gap', layerIds: [ids[0], ids[3]] }),
    /adjacent/i,
  );
  assert.throws(() => send('reorder-layer', { layerId: ids[0], beforeLayerId: ids[2] }), /Folder/i);
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  assert.equal(owner.historyView.future.length, history);
  send('redo');
  assert.equal(owner.snapshot().editor.layerFolders[0].name, 'Changed');
});

test('Duplicate inherits its Folder; deletion preserves empty Folder and creates last blank at root', () => {
  const { owner, send, ids } = fixture();
  const { folderId } = send('create-layer-folder', { name: 'Pair', layerIds: [ids[1], ids[2]] });
  const copy = send('duplicate-layer', { layerId: ids[1] });
  const folder = () => owner.snapshot().editor.layerFolders.find((item) => item.id === folderId);
  assert.ok(folder().members.some((member) => member.layerId === copy.layerId));
  send('delete-layers', { layerIds: [ids[1], ids[2], copy.layerId] });
  assert.deepEqual(folder().members, []);
  send('undo');
  assert.equal(folder().members.length, 3);
  send('redo');
  send('delete-layers', { layerIds: [ids[0], ids[3]] });
  assert.equal(owner.snapshot().layers.length, 1);
  assert.deepEqual(folder().members, []);
});

test('Project Import remaps Folder and Layer collisions together and Undo restores the destination', () => {
  const makeProject = (compositionId, layerId, folderId, includeLayer) => {
    const doc = blankDocument();
    doc.layers[0].id = layerId;
    const project = migrateLegacyDocumentToCompositionProject(doc);
    project.compositions[0].id = compositionId;
    const owner = createCompositionSessionOwner(project);
    owner.dispatch({
      ...owner.state,
      type: 'create-layer-folder',
      payload: {
        folderId,
        name: compositionId,
        layerIds: includeLayer ? [layerId] : [],
      },
    });
    return owner.projectSnapshot();
  };
  const destination = makeProject('destination', 'shared-layer', 'shared-folder', false),
    source = makeProject('source', 'shared-folder', 'shared-layer', true),
    owner = createCompositionSessionOwner(destination),
    before = owner.projectSnapshot();
  owner.dispatch({
    ...owner.state,
    type: 'import-project-compositions',
    payload: {
      sourceProject: source,
      compositionIds: ['source'],
    },
  });
  const imported = owner
    .projectSnapshot()
    .compositions.find(
      (item) => item.name === source.compositions[0].name && item.id !== 'destination',
    );
  assert.ok(imported);
  assert.notEqual(imported.layers[0].id, 'shared-folder');
  assert.notEqual(imported.editor.layerFolders[0].id, 'shared-layer');
  assert.deepEqual(imported.editor.layerFolders[0].members, [{ layerId: imported.layers[0].id }]);
  owner.dispatch({ ...owner.state, type: 'undo' });
  assert.deepEqual(owner.projectSnapshot(), before);
});

test('SVG replacement clears only the active Composition Folder and Undo restores it', () => {
  const { owner: sourceOwner, send, ids } = fixture();
  send('create-layer-folder', {
    folderId: 'first-folder',
    name: 'First',
    layerIds: [ids[1], ids[2]],
  });
  const project = sourceOwner.projectSnapshot();
  const other = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
  other.id = 'other-composition';
  other.editor.layerFolders = [
    { id: 'other-folder', name: 'Other', members: [{ layerId: other.layers[0].id }] },
  ];
  project.compositions.push(other);
  const owner = createCompositionSessionOwner(project),
    before = owner.projectSnapshot();
  const tree = readSVGSource(
    '<svg width="20" height="20"><rect width="20" height="20" fill="#ff0000"/></svg>',
    new Window().DOMParser,
  );
  owner.dispatch({ ...owner.state, type: 'import-svg', payload: { source: tree } });
  const after = owner.projectSnapshot();
  assert.equal(after.compositions[0].editor?.layerFolders, undefined);
  assert.deepEqual(after.compositions[1], before.compositions[1]);
  owner.dispatch({ ...owner.state, type: 'undo' });
  assert.deepEqual(owner.projectSnapshot(), before);
});

test('Grouping an SVG source and consumer keeps their reference and rendered output', () => {
  const document = blankDocument(),
    owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(document));
  const send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
  const consumerId = document.layers[0].id;
  const tree = readSVGSource(
    '<svg width="20" height="20"><rect width="20" height="20" fill="#ff0000"/></svg>',
    new Window().DOMParser,
  );
  const { sourceLayerId } = send('add-svg-part', {
    layerId: consumerId,
    source: tree,
    name: 'mark.svg',
  });
  const before = owner.snapshot(),
    svg = exportSVG(before);
  const { folderId } = send('create-layer-folder', {
    name: 'Referenced',
    layerIds: [consumerId, sourceLayerId],
  });
  assert.deepEqual(owner.snapshot().layers, before.layers);
  assert.equal(exportSVG(owner.snapshot()), svg);
  send('remove-layers-from-folder', { folderId, layerIds: [sourceLayerId] });
  assert.deepEqual(owner.snapshot().layers, before.layers);
  assert.equal(exportSVG(owner.snapshot()), svg);
});

test('Invalid inactive Folder membership and cross-Composition identity collisions reject atomically', () => {
  const { owner, send, ids } = fixture();
  send('create-layer-folder', {
    folderId: 'folder-one',
    name: 'Folder',
    layerIds: [ids[1], ids[2]],
  });
  const before = owner.projectSnapshot(),
    state = owner.state;
  const other = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
  other.id = 'other-composition';
  other.editor.layerFolders = [
    { id: 'other-folder', name: 'Other', members: [{ layerId: 'missing-layer' }] },
  ];
  const invalid = structuredClone(before);
  invalid.compositions.push(other);
  assert.throws(() => deserializeCompositionProject(JSON.stringify(invalid)), /Folder/i);
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  const overlapping = structuredClone(before);
  overlapping.compositions[0].editor.layerFolders.push({
    id: 'folder-two',
    name: 'Duplicate member',
    members: [{ layerId: ids[1] }],
  });
  assert.throws(() => deserializeCompositionProject(JSON.stringify(overlapping)), /Folder/i);
  const collision = structuredClone(before);
  other.editor.layerFolders[0].members = [{ layerId: other.layers[0].id }];
  other.layers[0].nodeGroups = [{ id: 'folder-one', name: 'Graph group', nodeIds: [] }];
  collision.compositions.push(other);
  assert.throws(() => deserializeCompositionProject(JSON.stringify(collision)), /Folder ID/i);
});
