import test from 'node:test';
import { remapIds } from '../src/project-import-remap.js';
import assert from 'node:assert/strict';
import { blankDocument, blankLayer, node, exportSVG, serialize, deserialize } from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  planLayerFolderMove,
  planLayerFolderOrder,
  planLayerFolderRemoval,
  validateLayerFolders,
  descendantLayerIds,
  canReorderLayerInFolders,
} from '../src/layer-folders.js';

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

test('nested sibling grouping and Ungroup preserve paint, IDs, History and both codecs', () => {
  const { owner, send, ids } = fixture();
  const before = owner.snapshot();
  const inner = send('create-layer-folder', { name: 'inner', layerIds: [ids[1]] }).folderId;
  const outer = send('create-layer-folder', {
    name: 'outer',
    layerIds: [ids[2]],
    folderIds: [inner],
  }).folderId;
  const snapshot = owner.snapshot();
  assert.deepEqual(snapshot.layers, before.layers);
  assert.equal(exportSVG(snapshot), exportSVG(before));
  assert.equal(snapshot.editor.layerFolders.find((f) => f.id === inner).parentId, outer);
  assert.deepEqual(new Set(descendantLayerIds(snapshot, outer)), new Set([ids[1], ids[2]]));
  const p = owner.projectSnapshot();
  assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(p)), p);
  send('dissolve-layer-folder', { folderId: inner });
  assert.equal(owner.snapshot().editor.layerFolders.length, 1);
  send('undo');
  assert.deepEqual(owner.snapshot(), snapshot);
  send('redo');
  send('undo');
  send('dissolve-layer-folder', { folderId: outer });
  const restored = owner.snapshot();
  assert.equal(restored.editor.layerFolders[0].id, inner);
  assert.equal(Object.hasOwn(restored.editor.layerFolders[0], 'parentId'), false);
  assert.deepEqual(restored.layers, before.layers);
});
test('nested creation refuses ancestors, gaps, locked descendants and rolls back', () => {
  const { owner, send, ids } = fixture();
  const inner = send('create-layer-folder', { name: 'inner', layerIds: [ids[1]] }).folderId;
  const outer = send('create-layer-folder', { name: 'outer', folderIds: [inner] }).folderId;
  const before = owner.projectSnapshot();
  for (const payload of [
    { folderIds: [inner, outer] },
    { folderIds: [inner], layerIds: [ids[0]] },
    { folderIds: [outer], layerIds: [ids[3]] },
  ]) {
    assert.throws(() => send('create-layer-folder', { name: 'bad', ...payload }));
    assert.deepEqual(owner.projectSnapshot(), before);
  }
  const locked = fixture({}, (doc) => {
    doc.layers[1].locked = true;
    doc.editor.layerFolders = [
      { id: 'inner', name: 'inner', members: [{ layerId: doc.layers[1].id }] },
    ];
  });
  assert.throws(
    () => locked.send('create-layer-folder', { name: 'bad', folderIds: ['inner'] }),
    /Unlock/,
  );
  assert.throws(() => locked.send('dissolve-layer-folder', { folderId: 'inner' }), /Unlock/);
});
test('parent graph and descendant spans are validated, empty folders survive', () => {
  const { owner, send, ids } = fixture();
  const a = send('create-layer-folder', { name: 'empty' }).folderId;
  const b = send('create-layer-folder', { name: 'outer', folderIds: [a] }).folderId;
  const base = owner.snapshot();
  validateLayerFolders(base);
  for (const change of [
    (d) => (d.editor.layerFolders[0].parentId = 'missing'),
    (d) => (d.editor.layerFolders[1].parentId = a),
    (d) => (d.editor.layerFolders[0].parentId = null),
    (d) => {
      d.editor.layerFolders[0].members = [{ layerId: ids[0] }, { layerId: ids[2] }];
    },
  ]) {
    const bad = structuredClone(base);
    change(bad);
    assert.throws(() => validateLayerFolders(bad));
  }
  send('dissolve-layer-folder', { folderId: b });
  assert.equal(owner.snapshot().editor.layerFolders[0].id, a);
});
test('flat moves cannot cross a nested branch even when endpoints are flat', () => {
  const { owner, send, ids } = fixture();
  const a = send('create-layer-folder', { name: 'a', layerIds: [ids[0]] }).folderId;
  const inner = send('create-layer-folder', { name: 'nested', layerIds: [ids[1]] }).folderId;
  send('create-layer-folder', { name: 'outer', folderIds: [inner] });
  const target = send('create-layer-folder', { name: 'target', layerIds: [ids[3]] }).folderId;
  const d = owner.snapshot();
  assert.throws(() => planLayerFolderMove(d, { folderId: target, layerIds: [ids[0]] }), /nested/);
  assert.throws(() => planLayerFolderOrder(d, { folderId: a, direction: 'forward' }), /nested/);
  assert.throws(() => planLayerFolderRemoval(d, { folderId: inner, layerIds: [ids[1]] }), /Nested/);
});

test('nested duplicate, last deletion, safe direct reorder and legacy codec preserve ancestry', () => {
  const { owner, send, ids } = fixture();
  const child = send('create-layer-folder', { name: 'child', layerIds: [ids[1], ids[2]] }).folderId;
  const parent = send('create-layer-folder', { name: 'parent', folderIds: [child] }).folderId;
  const original = owner.snapshot();
  assert.deepEqual(deserialize(serialize(original)), original);
  assert.equal(canReorderLayerInFolders(original, ids[1], 2), true);
  send('reorder-layer', { layerId: ids[1], beforeLayerId: ids[3] });
  assert.deepEqual(
    owner.snapshot().layers.map((l) => l.id),
    [ids[0], ids[2], ids[1], ids[3]],
  );
  send('undo');
  assert.deepEqual(owner.snapshot(), original);
  assert.equal(canReorderLayerInFolders(original, ids[1], 0), false);
  const copy = send('duplicate-layer', { layerId: ids[1] });
  assert.equal(owner.snapshot().editor.layerFolders.find((f) => f.id === child).parentId, parent);
  assert.ok(descendantLayerIds(owner.snapshot(), parent).includes(copy.layerId));
  send('delete-layers', { layerIds: [ids[1], ids[2], copy.layerId] });
  assert.deepEqual(owner.snapshot().editor.layerFolders, [
    { id: child, name: 'child', members: [], parentId: parent },
    { id: parent, name: 'parent', members: [] },
  ]);
  send('undo');
  assert.ok(descendantLayerIds(owner.snapshot(), parent).includes(copy.layerId));
});

test('unrelated flat movement works beside a parent whose direct members have a child interval', () => {
  const { owner, send, ids } = fixture();
  const child = send('create-layer-folder', { name: 'child', layerIds: [ids[1]] }).folderId;
  send('create-layer-folder', { name: 'parent', folderIds: [child], layerIds: [ids[0], ids[2]] });
  const empty = send('create-layer-folder', { name: 'unrelated' }).folderId;
  const before = owner.snapshot();
  const plan = planLayerFolderMove(before, { folderId: empty, layerIds: [ids[3]] });
  send('move-layers-to-folder', {
    folderId: empty,
    layerIds: [ids[3]],
    expectedPaintOrder: plan.paintOrder,
  });
  assert.deepEqual(owner.snapshot().layers, before.layers);
  assert.deepEqual(
    owner.snapshot().editor.layerFolders.slice(0, 2),
    before.editor.layerFolders.slice(0, 2),
  );
});

test('nested import collision remaps Folder parents and direct members together, preserving source', () => {
  const { owner, send, ids } = fixture();
  const inner = send('create-layer-folder', { name: 'inner', layerIds: [ids[1]] }).folderId;
  const outer = send('create-layer-folder', {
    name: 'outer',
    folderIds: [inner],
    layerIds: [ids[2]],
  }).folderId;
  const before = owner.projectSnapshot(),
    source = structuredClone(before);
  const compositionId = source.compositions[0].id;
  send('import-project-compositions', { sourceProject: source, compositionIds: [compositionId] });
  const after = owner.projectSnapshot(),
    imported = after.compositions[1];
  assert.deepEqual(source, before);
  assert.deepEqual(after.compositions[0], before.compositions[0]);
  assert.equal(imported.id, compositionId + '-import-1');
  const folders = imported.editor.layerFolders;
  assert.equal(folders[0].id, inner + '-import-1');
  assert.equal(folders[0].parentId, outer + '-import-1');
  assert.equal(folders[1].id, outer + '-import-1');
  assert.deepEqual(folders[0].members, [{ layerId: ids[1] + '-import-1' }]);
  assert.deepEqual(folders[1].members, [{ layerId: ids[2] + '-import-1' }]);
  assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(after)), after);
  send('undo');
  assert.deepEqual(owner.projectSnapshot(), before);
  send('redo');
  assert.deepEqual(owner.projectSnapshot(), after);
  const broken = structuredClone(source);
  broken.compositions[0].editor.layerFolders[0].parentId = 'foreign';
  assert.throws(() =>
    send('import-project-compositions', { sourceProject: broken, compositionIds: [compositionId] }),
  );
  assert.deepEqual(owner.projectSnapshot(), after);
});

test('100-depth bound and malformed parent graphs fail closed without unbounded recursion', () => {
  const { owner } = fixture(),
    doc = owner.snapshot();
  doc.editor.layerFolders = Array.from({ length: 100 }, (_, i) => ({
    id: 'f' + i,
    name: 'Folder',
    members: [],
    ...(i ? { parentId: 'f' + (i - 1) } : {}),
  }));
  validateLayerFolders(doc);
  assert.deepEqual(deserialize(serialize(doc)), doc);
  doc.editor.layerFolders.push({ id: 'f100', name: 'too deep', members: [], parentId: 'f99' });
  assert.throws(() => validateLayerFolders(doc));
  doc.editor.layerFolders.pop();
  doc.editor.layerFolders[0].parentId = 'f99';
  assert.throws(() => validateLayerFolders(doc), /cycle/);
  delete doc.editor.layerFolders[0].parentId;
  doc.editor.layerFolders[1].members = [{ layerId: 'foreign' }];
  assert.throws(() => validateLayerFolders(doc), /membership/);
});

test('Folder parent remap does not reinterpret opaque metadata IDs', () => {
  const value = { id: 'child', parentId: 'parent', metadata: { parentId: 'parent', id: 'child' } };
  const before = structuredClone(value);
  const remapped = remapIds(value, new Set(['child', 'parent']), new Set(['child', 'parent']), 60);
  assert.equal(remapped.id, 'child-import-1');
  assert.equal(remapped.parentId, 'parent-import-1');
  assert.deepEqual(remapped.metadata, value.metadata);
  assert.deepEqual(value, before);
});
