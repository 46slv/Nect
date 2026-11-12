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
import { planLayerFolderMove, planLayerFolderRemoval } from '../src/layer-folders.js';

function fixture(options = {}, customize = () => {}) {
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
  customize(doc);
  const owner = createCompositionSessionOwner(
    migrateLegacyDocumentToCompositionProject(doc),
    options,
  );
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

test('Folder moves keep paint order when membership alone stays contiguous', () => {
  const { owner, send, ids } = fixture();
  const { folderId: target } = send('create-layer-folder', { name: 'Target', layerIds: [ids[1]] });
  const { folderId: source } = send('create-layer-folder', {
    name: 'Source',
    layerIds: [ids[2], ids[3]],
  });
  const before = owner.snapshot(),
    svg = exportSVG(before),
    plan = planLayerFolderMove(before, { folderId: target, layerIds: [ids[2], ids[0]] });
  assert.equal(plan.orderChanged, false);
  const result = send('move-layers-to-folder', { folderId: target, layerIds: [ids[2], ids[0]] });
  assert.equal(result.changed, true);
  const moved = owner.snapshot();
  assert.deepEqual(moved.layers, before.layers);
  assert.equal(exportSVG(moved), svg);
  assert.deepEqual(moved.editor.layerFolders.find((folder) => folder.id === target).members, [
    { layerId: ids[0] },
    { layerId: ids[1] },
    { layerId: ids[2] },
  ]);
  assert.deepEqual(moved.editor.layerFolders.find((folder) => folder.id === source).members, [
    { layerId: ids[3] },
  ]);
  const noOp = send('move-layers-to-folder', { folderId: target, layerIds: [ids[2], ids[0]] });
  assert.equal(noOp.changed, false);
});

test('Folder moves require the exact proposed order and preserve IDs across Undo, Redo and reopen', () => {
  const { owner, send, ids } = fixture();
  const { folderId: target } = send('create-layer-folder', { name: 'Target', layerIds: [ids[0]] });
  const { folderId: source } = send('create-layer-folder', {
    name: 'Source',
    layerIds: [ids[1], ids[2]],
  });
  send('rename-layer-folder', { folderId: source, name: 'Changed' });
  send('undo');
  const before = owner.projectSnapshot(),
    state = owner.state,
    future = owner.historyView.future.length,
    payload = { folderId: target, layerIds: [ids[3], ids[2], ids[0]] },
    plan = planLayerFolderMove(owner.snapshot(), payload);
  assert.equal(plan.orderChanged, true);
  assert.deepEqual(plan.paintOrder, [ids[0], ids[2], ids[3], ids[1]]);
  assert.throws(() => send('move-layers-to-folder', payload), /Confirm/);
  assert.throws(
    () => send('move-layers-to-folder', { ...payload, expectedPaintOrder: ids }),
    /Confirm/,
  );
  assert.throws(
    () => send('move-layers-to-folder', { ...payload, expectedPaintOrder: Array(ids.length) }),
    /Confirm/,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  assert.equal(owner.historyView.future.length, future);
  const result = send('move-layers-to-folder', {
    ...payload,
    expectedPaintOrder: plan.paintOrder,
  });
  assert.equal(result.changed, true);
  const moved = owner.projectSnapshot(),
    doc = owner.snapshot();
  assert.deepEqual(
    doc.layers.map(({ id }) => id),
    plan.paintOrder,
  );
  assert.deepEqual(
    doc.layers.map((layer) => layer.nodes),
    plan.paintOrder.map(
      (id) => before.compositions[0].layers.find((layer) => layer.id === id).nodes,
    ),
  );
  assert.deepEqual(doc.editor.layerFolders.find((folder) => folder.id === source).members, [
    { layerId: ids[1] },
  ]);
  assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(moved)), moved);
  send('undo');
  assert.deepEqual(owner.projectSnapshot(), before);
  send('redo');
  assert.deepEqual(owner.projectSnapshot(), moved);
  assert.throws(
    () => owner.dispatch({ ...state, type: 'move-layers-to-folder', payload }),
    /STALE/,
  );
});

test('Folder moves reject locked or foreign Layers without changing the project or Redo', () => {
  const { owner, send, ids } = fixture({}, (doc) => {
    doc.layers[2].locked = true;
    doc.layers[1].visible = false;
  });
  const { folderId } = send('create-layer-folder', { name: 'Target', layerIds: [ids[0]] });
  send('rename-layer-folder', { folderId, name: 'Temporary' });
  send('undo');
  const before = owner.projectSnapshot(),
    state = owner.state,
    future = owner.historyView.future;
  for (const layerIds of [[ids[2]], [ids[1], 'other-composition-layer'], [ids[1], ids[1]]]) {
    assert.throws(() => send('move-layers-to-folder', { folderId, layerIds }), /Layer|Unlock/);
    assert.deepEqual(owner.projectSnapshot(), before);
    assert.deepEqual(owner.state, state);
    assert.deepEqual(owner.historyView.future, future);
  }
  send('redo');
  assert.equal(owner.snapshot().editor.layerFolders[0].name, 'Temporary');
});

test('Moving into an empty Folder places the selected block at the Composition front', () => {
  const { owner, send, ids } = fixture();
  const { folderId } = send('create-layer-folder', { name: 'Empty', layerIds: [] });
  const payload = { folderId, layerIds: [ids[2], ids[0]] },
    plan = planLayerFolderMove(owner.snapshot(), payload);
  assert.deepEqual(plan.paintOrder, [ids[1], ids[3], ids[0], ids[2]]);
  assert.throws(() => send('move-layers-to-folder', payload), /Confirm/);
  send('move-layers-to-folder', { ...payload, expectedPaintOrder: plan.paintOrder });
  assert.deepEqual(owner.snapshot().editor.layerFolders[0].members, [
    { layerId: ids[0] },
    { layerId: ids[2] },
  ]);
});

test('Moving middle members out preserves both relative orders and neighboring Folders', () => {
  const { owner, send, ids } = fixture({}, (doc) => {
    doc.layers.push(blankLayer('Fifth'), blankLayer('Sixth'));
  });
  const { folderId } = send('create-layer-folder', {
    name: 'Source',
    layerIds: ids.slice(0, 5),
  });
  const { folderId: neighbor } = send('create-layer-folder', {
    name: 'Neighbor',
    layerIds: [ids[5]],
  });
  send('rename-layer-folder', { folderId, name: 'Temporary' });
  send('undo');
  const before = owner.projectSnapshot(),
    state = owner.state,
    future = owner.historyView.future,
    payload = { folderId, layerIds: [ids[3], ids[1]] },
    plan = planLayerFolderRemoval(owner.snapshot(), payload);
  assert.equal(plan.orderChanged, true);
  assert.deepEqual(plan.paintOrder, [ids[0], ids[2], ids[4], ids[1], ids[3], ids[5]]);
  assert.throws(() => send('remove-layers-from-folder', payload), /Confirm/);
  assert.throws(
    () => send('remove-layers-from-folder', { ...payload, expectedPaintOrder: ids }),
    /Confirm/,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  assert.deepEqual(owner.historyView.future, future);
  send('remove-layers-from-folder', { ...payload, expectedPaintOrder: plan.paintOrder });
  const moved = owner.projectSnapshot(),
    document = owner.snapshot();
  assert.deepEqual(
    document.layers.map(({ id }) => id),
    plan.paintOrder,
  );
  assert.deepEqual(document.editor.layerFolders.find((entry) => entry.id === folderId).members, [
    { layerId: ids[0] },
    { layerId: ids[2] },
    { layerId: ids[4] },
  ]);
  assert.deepEqual(document.editor.layerFolders.find((entry) => entry.id === neighbor).members, [
    { layerId: ids[5] },
  ]);
  assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(moved)), moved);
  send('undo');
  assert.deepEqual(owner.projectSnapshot(), before);
  send('redo');
  assert.deepEqual(owner.projectSnapshot(), moved);
});

test('Moving both edges or all members out leaves paint order unchanged', () => {
  const { owner, send, ids } = fixture();
  const { folderId } = send('create-layer-folder', { name: 'Source', layerIds: ids.slice(0, 3) });
  const before = owner.snapshot(),
    payload = { folderId, layerIds: [ids[2], ids[0]] },
    plan = planLayerFolderRemoval(before, payload);
  assert.equal(plan.orderChanged, false);
  send('remove-layers-from-folder', payload);
  assert.deepEqual(owner.snapshot().layers, before.layers);
  assert.deepEqual(owner.snapshot().editor.layerFolders[0].members, [{ layerId: ids[1] }]);
  send('undo');
  const all = { folderId, layerIds: ids.slice(0, 3) };
  assert.equal(planLayerFolderRemoval(owner.snapshot(), all).orderChanged, false);
  send('remove-layers-from-folder', all);
  assert.deepEqual(owner.snapshot().layers, before.layers);
  assert.deepEqual(owner.snapshot().editor.layerFolders[0].members, []);
});

test('Moving out rejects locked, root, foreign and duplicate IDs atomically', () => {
  const { owner, send, ids } = fixture({}, (doc) => {
    doc.layers[1].locked = true;
    doc.editor.layerFolders = [
      {
        id: 'source',
        name: 'Source',
        members: [{ layerId: doc.layers[0].id }, { layerId: doc.layers[1].id }],
      },
    ];
  });
  send('rename-layer-folder', { folderId: 'source', name: 'Temporary' });
  send('undo');
  const before = owner.projectSnapshot(),
    state = owner.state,
    future = owner.historyView.future;
  for (const layerIds of [[ids[1]], [ids[0], ids[2]], [ids[0], 'other'], [ids[0], ids[0]]]) {
    assert.throws(
      () => send('remove-layers-from-folder', { folderId: 'source', layerIds }),
      /Layer|Unlock/,
    );
    assert.deepEqual(owner.projectSnapshot(), before);
    assert.deepEqual(owner.state, state);
    assert.deepEqual(owner.historyView.future, future);
  }
  send('redo');
  assert.equal(owner.snapshot().editor.layerFolders[0].name, 'Temporary');
});

for (const direction of ['into', 'out of'])
  test(`A Text draft started while moving ${direction} a Folder remains editable`, () => {
    let owner,
      textId,
      layerId,
      draft,
      armed = false;
    const options = {
      fonts: () => {
        if (armed) {
          armed = false;
          draft = owner.dispatch({
            ...owner.state,
            type: 'begin-text-draft',
            payload: { layerId, nodeId: textId },
          });
          draft = owner.dispatch({
            ...owner.state,
            type: 'stage-text-draft',
            payload: {
              draftId: draft.draftId,
              expectedDraftRevision: draft.draftRevision,
              action: 'edit',
              previousRaw: 'A',
              nextRaw: 'AB',
            },
          });
        }
        return {};
      },
    };
    const f = fixture(options, (doc) => {
      const text = node('Text');
      text.text = 'A';
      doc.layers[0].nodes.push(text);
      textId = text.id;
      layerId = doc.layers[0].id;
    });
    owner = f.owner;
    const { folderId } = f.send('create-layer-folder', {
      name: 'Target',
      layerIds: direction === 'into' ? [f.ids[0]] : f.ids.slice(0, 3),
    });
    f.send('rename-layer-folder', { folderId, name: 'Temporary' });
    f.send('undo');
    const before = owner.projectSnapshot(),
      state = owner.state,
      future = owner.historyView.future,
      payload = { folderId, layerIds: [direction === 'into' ? f.ids[3] : f.ids[1]] },
      plan = (direction === 'into' ? planLayerFolderMove : planLayerFolderRemoval)(
        owner.snapshot(),
        payload,
      ),
      type = direction === 'into' ? 'move-layers-to-folder' : 'remove-layers-from-folder';
    armed = true;
    assert.throws(
      () => f.send(type, { ...payload, expectedPaintOrder: plan.paintOrder }),
      /draft|gesture|active/i,
    );
    assert.deepEqual(owner.projectSnapshot(), before);
    assert.deepEqual(owner.state, state);
    assert.deepEqual(owner.historyView.future, future);
    f.send('commit-text-draft', {
      draftId: draft.draftId,
      expectedDraftRevision: draft.draftRevision,
    });
    assert.equal(owner.snapshot().layers[0].nodes.find((item) => item.id === textId).text, 'AB');
  });

test('Move validation cannot publish after the active Composition changes reentrantly', () => {
  const f = fixture(),
    project = f.owner.projectSnapshot(),
    other = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
  other.id = 'other-composition';
  project.compositions.push(other);
  let owner,
    armed = false;
  owner = createCompositionSessionOwner(project, {
    fonts: () => {
      if (armed) {
        armed = false;
        owner.switchComposition(other.id);
      }
      return {};
    },
  });
  const send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
  const { folderId } = send('create-layer-folder', { name: 'Target', layerIds: [f.ids[0]] });
  const before = owner.projectSnapshot(),
    payload = { folderId, layerIds: [f.ids[3]] },
    plan = planLayerFolderMove(owner.snapshot(), payload);
  armed = true;
  assert.throws(
    () => send('move-layers-to-folder', { ...payload, expectedPaintOrder: plan.paintOrder }),
    /STALE/,
  );
  assert.equal(owner.activeCompositionId, other.id);
  assert.deepEqual(owner.projectSnapshot(), before);
});

test('Moving a hidden referenced SVG source preserves its stable reference and output', () => {
  const { owner, send, ids } = fixture();
  const tree = readSVGSource(
    '<svg width="20" height="20"><rect width="20" height="20" fill="#ff0000"/></svg>',
    new Window().DOMParser,
  );
  const { sourceLayerId, nodeId } = send('add-svg-part', {
    layerId: ids[1],
    source: tree,
    name: 'mark.svg',
  });
  const { folderId } = send('create-layer-folder', { name: 'Target', layerIds: [ids[0]] });
  const before = owner.snapshot(),
    beforeSvg = exportSVG(before),
    payload = { folderId, layerIds: [sourceLayerId] },
    plan = planLayerFolderMove(before, payload);
  assert.equal(plan.orderChanged, true);
  send('move-layers-to-folder', { ...payload, expectedPaintOrder: plan.paintOrder });
  const moved = owner.snapshot();
  assert.equal(moved.layers.find((layer) => layer.id === sourceLayerId).visible, false);
  assert.deepEqual(
    moved.layers.find((layer) => layer.id === ids[1]).nodes.find((item) => item.id === nodeId)
      .compositionReference,
    { compositionId: sourceLayerId },
  );
  assert.equal(exportSVG(moved), beforeSvg);
  send('undo');
  assert.deepEqual(owner.snapshot(), before);
  send('redo');
  assert.deepEqual(owner.snapshot(), moved);
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
