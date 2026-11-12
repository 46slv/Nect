import test from 'node:test';
import assert from 'node:assert/strict';
import { orderLayerFolder, planLayerFolderOrder } from '../src/layer-folders.js';

function fixture() {
  return {
    layers: ['back', 'a1', 'a2', 'b1', 'b2', 'front'].map((id) => ({
      id,
      nodes: [{ id: `node-${id}`, value: [id] }],
    })),
    editor: {
      layerFolders: [
        { id: 'a', name: 'A', members: [{ layerId: 'a1' }, { layerId: 'a2' }] },
        { id: 'b', name: 'B', members: [{ layerId: 'b1' }, { layerId: 'b2' }] },
        { id: 'empty', name: 'Empty', members: [] },
      ],
    },
  };
}

test('Folder order plans move complete neighboring blocks without touching the source', () => {
  const doc = fixture(),
    before = structuredClone(doc),
    forward = planLayerFolderOrder(doc, { folderId: 'a', direction: 'forward' }),
    backward = planLayerFolderOrder(doc, { folderId: 'a', direction: 'backward' });
  assert.deepEqual(forward.paintOrder, ['back', 'b1', 'b2', 'a1', 'a2', 'front']);
  assert.deepEqual(backward.paintOrder, ['a1', 'a2', 'back', 'b1', 'b2', 'front']);
  assert.deepEqual(forward.layerIds, ['a1', 'a2']);
  assert.equal(forward.orderChanged, true);
  assert.deepEqual(doc, before);
});

test('Folder order crosses one neighboring root row in either direction', () => {
  const doc = fixture();
  const cases = [
    ['a', 'backward', ['a1', 'a2', 'back', 'b1', 'b2', 'front']],
    ['a', 'forward', ['back', 'b1', 'b2', 'a1', 'a2', 'front']],
    ['b', 'backward', ['back', 'b1', 'b2', 'a1', 'a2', 'front']],
    ['b', 'forward', ['back', 'a1', 'a2', 'front', 'b1', 'b2']],
  ];
  for (const [folderId, direction, expected] of cases) {
    const plan = planLayerFolderOrder(doc, { folderId, direction });
    assert.deepEqual(plan.paintOrder, expected);
    assert.equal(plan.orderChanged, true);
  }
});

test('confirmed Folder order changes only the Layer permutation and preserves identities', () => {
  const doc = fixture(),
    layers = new Map(doc.layers.map((layer) => [layer.id, layer])),
    folders = structuredClone(doc.editor.layerFolders),
    payload = { folderId: 'a', direction: 'forward' },
    expectedPaintOrder = planLayerFolderOrder(doc, payload).paintOrder;
  assert.deepEqual(orderLayerFolder(doc, { ...payload, expectedPaintOrder }), {
    folderId: 'a',
    orderChanged: true,
  });
  assert.deepEqual(
    doc.layers.map((layer) => layer.id),
    expectedPaintOrder,
  );
  assert.deepEqual(doc.editor.layerFolders, folders);
  for (const layer of doc.layers) assert.strictEqual(layer, layers.get(layer.id));
});

test('changed or missing confirmation rejects a Folder move without mutation', () => {
  const doc = fixture(),
    before = structuredClone(doc),
    payload = { folderId: 'a', direction: 'forward' };
  assert.throws(() => orderLayerFolder(doc, payload), /Confirm the changed Layer order/);
  assert.throws(
    () => orderLayerFolder(doc, { ...payload, expectedPaintOrder: ['back', 'a1'] }),
    /Confirm the changed Layer order/,
  );
  const stale = planLayerFolderOrder(doc, payload).paintOrder;
  [stale[1], stale[2]] = [stale[2], stale[1]];
  assert.throws(
    () => orderLayerFolder(doc, { ...payload, expectedPaintOrder: stale }),
    /Confirm the changed Layer order/,
  );
  assert.deepEqual(doc, before);
});

test('locked members refuse a Folder move but a locked neighboring block may be crossed', () => {
  const doc = fixture();
  doc.layers.find((layer) => layer.id === 'a2').locked = true;
  const before = structuredClone(doc);
  assert.throws(() => planLayerFolderOrder(doc, { folderId: 'a', direction: 'forward' }), /Unlock/);
  assert.deepEqual(doc, before);
  doc.layers.find((layer) => layer.id === 'a2').locked = false;
  doc.layers.find((layer) => layer.id === 'b1').locked = true;
  const payload = { folderId: 'a', direction: 'forward' },
    plan = planLayerFolderOrder(doc, payload);
  orderLayerFolder(doc, { ...payload, expectedPaintOrder: plan.paintOrder });
  assert.equal(doc.layers.find((layer) => layer.id === 'b1').locked, true);
  assert.deepEqual(
    doc.layers.map((layer) => layer.id),
    plan.paintOrder,
  );
});

test('empty and boundary Folders are no-ops; invalid or stale targets refuse', () => {
  const doc = fixture(),
    originalLayers = doc.layers,
    before = structuredClone(doc);
  for (const payload of [
    { folderId: 'empty', direction: 'forward' },
    { folderId: 'empty', direction: 'backward' },
  ]) {
    const plan = planLayerFolderOrder(doc, payload);
    assert.equal(plan.orderChanged, false);
    assert.deepEqual(orderLayerFolder(doc, payload), { folderId: 'empty', orderChanged: false });
    assert.throws(
      () => orderLayerFolder(doc, { ...payload, expectedPaintOrder: [] }),
      /Confirm the changed Layer order/,
    );
  }
  assert.throws(
    () => planLayerFolderOrder(doc, { folderId: 'gone', direction: 'forward' }),
    /unavailable/,
  );
  assert.throws(
    () => planLayerFolderOrder(doc, { folderId: 'a', direction: 'sideways' }),
    /Forward or Backward/,
  );
  assert.deepEqual(doc, before);
  assert.strictEqual(doc.layers, originalLayers);

  const edge = fixture();
  edge.layers.shift();
  const edgeLayers = edge.layers;
  assert.equal(
    planLayerFolderOrder(edge, { folderId: 'a', direction: 'backward' }).orderChanged,
    false,
  );
  assert.equal(
    orderLayerFolder(edge, { folderId: 'a', direction: 'backward' }).orderChanged,
    false,
  );
  assert.strictEqual(edge.layers, edgeLayers);
});

test('invalid noncontiguous membership refuses without altering either block', () => {
  const doc = fixture();
  doc.editor.layerFolders[0].members[1].layerId = 'front';
  const before = structuredClone(doc);
  assert.throws(
    () => planLayerFolderOrder(doc, { folderId: 'a', direction: 'forward' }),
    /contiguous/,
  );
  assert.deepEqual(doc, before);
});
