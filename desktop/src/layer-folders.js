import { guideDefaults } from './guides.js';

// Folders organize the existing paint-ordered Layer array. They never render or
// alter a Layer; membership is a set of exact Layer identities in one Composition.
export const layerFolders = (doc) => doc.editor?.layerFolders || [];

export function folderForLayer(doc, layerId) {
  return layerFolders(doc).find((folder) =>
    folder.members.some((member) => member.layerId === layerId),
  );
}

export function validateLayerFolders(
  doc,
  validateId = (id) => {
    if (typeof id !== 'string' || !id) throw Error('Invalid Folder ID');
  },
) {
  const folders = doc.editor?.layerFolders;
  if (folders === undefined) return;
  if (!Array.isArray(folders) || folders.length > 100 || !Array.isArray(doc.layers))
    throw Error('Invalid Layer folders');
  const layerIndices = new Map(doc.layers.map((layer, index) => [layer.id, index]));
  const folderIds = new Set(),
    assigned = new Set();
  for (const folder of folders) {
    if (!folder || typeof folder !== 'object' || Array.isArray(folder))
      throw Error('Invalid Folder');
    if (Object.keys(folder).some((key) => !['id', 'name', 'members'].includes(key)))
      throw Error('Invalid Folder field');
    validateId(folder.id);
    if (folderIds.has(folder.id)) throw Error('Duplicate Folder ID');
    folderIds.add(folder.id);
    if (typeof folder.name !== 'string' || !folder.name.trim() || folder.name.length > 100)
      throw Error('Folder name must contain 1–100 characters');
    if (!Array.isArray(folder.members) || folder.members.length > doc.layers.length)
      throw Error('Invalid Folder members');
    const indices = [];
    for (const member of folder.members) {
      if (
        !member ||
        typeof member !== 'object' ||
        Array.isArray(member) ||
        Object.keys(member).some((key) => key !== 'layerId') ||
        typeof member.layerId !== 'string' ||
        !layerIndices.has(member.layerId) ||
        assigned.has(member.layerId)
      )
        throw Error('Invalid Folder Layer membership');
      assigned.add(member.layerId);
      indices.push(layerIndices.get(member.layerId));
    }
    indices.sort((a, b) => a - b);
    if (indices.some((index, i) => i > 0 && index !== indices[i - 1] + 1))
      throw Error('Folder Layers must remain contiguous');
  }
}

function folderName(name) {
  if (typeof name !== 'string' || !name.trim() || name.trim().length > 100)
    throw Error('Folder name must contain 1–100 characters');
  return name.trim();
}

function selectedLayers(doc, ids) {
  if (!Array.isArray(ids) || new Set(ids).size !== ids.length)
    throw Error('Select distinct Layers');
  const wanted = new Set(ids),
    selected = doc.layers.filter((layer) => wanted.has(layer.id));
  if (selected.length !== ids.length) throw Error('Layer is unavailable; select it again');
  if (selected.some((layer) => layer.locked)) throw Error('Unlock selected Layers first');
  return selected;
}

function selectedRootLayers(doc, ids) {
  const selected = selectedLayers(doc, ids);
  if (selected.some((layer) => folderForLayer(doc, layer.id)))
    throw Error('Move selected Layers out of their Folder first');
  return selected;
}

function contiguous(doc, layers) {
  if (!layers.length) return true;
  const selected = new Set(layers.map((layer) => layer.id));
  const indices = doc.layers.flatMap((layer, index) => (selected.has(layer.id) ? [index] : []));
  return indices.at(-1) - indices[0] + 1 === indices.length;
}

function findFolder(doc, folderId) {
  const folder = layerFolders(doc).find((item) => item.id === folderId);
  if (!folder) throw Error('Folder is unavailable');
  return folder;
}

export function createLayerFolder(doc, { folderId, name, layerIds = [] }) {
  const selected = selectedRootLayers(doc, layerIds);
  if (!contiguous(doc, selected)) throw Error('Select adjacent Layers to create a Folder');
  if (layerFolders(doc).length >= 100) throw Error('Maximum 100 Folders');
  const folder = {
    id: folderId,
    name: folderName(name),
    members: selected.map(({ id }) => ({ layerId: id })),
  };
  doc.editor ||= guideDefaults();
  doc.editor.layerFolders ||= [];
  doc.editor.layerFolders.push(folder);
  return { folderId };
}

export function renameLayerFolder(doc, { folderId, name, expectedName }) {
  const folder = findFolder(doc, folderId);
  if (expectedName !== undefined && folder.name !== expectedName)
    throw Error('Folder name changed; rename it again');
  folder.name = folderName(name);
  return { folderId };
}

export function addLayersToFolder(doc, { folderId, layerIds }) {
  const folder = findFolder(doc, folderId),
    selected = selectedRootLayers(doc, layerIds);
  if (!selected.length) throw Error('Select Layers to add');
  const ids = new Set(folder.members.map((member) => member.layerId));
  for (const layer of selected) ids.add(layer.id);
  const all = doc.layers.filter((layer) => ids.has(layer.id));
  if (!contiguous(doc, all)) throw Error('Select Layers adjacent to this Folder');
  folder.members = all.map(({ id }) => ({ layerId: id }));
  return { folderId };
}

// Plan without changing the document so the exact paint order can be shown and
// approved before the Session applies an order-changing move.
export function planLayerFolderMove(doc, { folderId, layerIds }) {
  const target = findFolder(doc, folderId),
    selected = selectedLayers(doc, layerIds);
  if (!selected.length) throw Error('Select Layers to move');
  const wanted = new Set(selected.map(({ id }) => id));
  const folders = layerFolders(doc);
  const members = new Map(
    folders.map((folder) => [
      folder.id,
      new Set(
        folder.members
          .filter((member) => folder.id === folderId || !wanted.has(member.layerId))
          .map((member) => member.layerId),
      ),
    ]),
  );
  for (const layer of selected) members.get(folderId).add(layer.id);
  const isContiguous = (layers, ids) => {
    const indexes = layers.flatMap((layer, index) => (ids.has(layer.id) ? [index] : []));
    return indexes.length < 2 || indexes.at(-1) - indexes[0] + 1 === indexes.length;
  };
  let layers = doc.layers;
  if (folders.some((folder) => !isContiguous(layers, members.get(folder.id)))) {
    const remaining = doc.layers.filter((layer) => !wanted.has(layer.id));
    const oldTarget = new Set(target.members.map((member) => member.layerId));
    const targetPositions = remaining.flatMap((layer, index) =>
      oldTarget.has(layer.id) ? [index] : [],
    );
    let insertAt;
    if (targetPositions.length) insertAt = targetPositions.at(-1) + 1;
    else if (oldTarget.size) {
      const lastTarget = doc.layers.findLastIndex((layer) => oldTarget.has(layer.id));
      insertAt = doc.layers
        .slice(0, lastTarget + 1)
        .filter((layer) => !wanted.has(layer.id)).length;
    } else insertAt = remaining.length;
    layers = [...remaining.slice(0, insertAt), ...selected, ...remaining.slice(insertAt)];
  }
  if (folders.some((folder) => !isContiguous(layers, members.get(folder.id))))
    throw Error('Cannot keep Folder Layers together');
  const paintOrder = layers.map(({ id }) => id);
  return {
    paintOrder,
    orderChanged: paintOrder.some((id, index) => id !== doc.layers[index].id),
    members: folders.map((folder) => ({
      folderId: folder.id,
      layerIds: layers.filter((layer) => members.get(folder.id).has(layer.id)).map(({ id }) => id),
    })),
  };
}

export function moveLayersToFolder(doc, payload) {
  const plan = planLayerFolderMove(doc, payload);
  if (
    (plan.orderChanged || payload.expectedPaintOrder !== undefined) &&
    (!Array.isArray(payload.expectedPaintOrder) ||
      payload.expectedPaintOrder.length !== plan.paintOrder.length ||
      plan.paintOrder.some((id, index) => payload.expectedPaintOrder[index] !== id))
  )
    throw Error('Confirm the changed Layer order before moving');
  const byId = new Map(doc.layers.map((layer) => [layer.id, layer]));
  doc.layers = plan.paintOrder.map((id) => byId.get(id));
  for (const entry of plan.members) {
    const folder = findFolder(doc, entry.folderId);
    const old = new Set(folder.members.map((member) => member.layerId));
    if (old.size !== entry.layerIds.length || entry.layerIds.some((id) => !old.has(id)))
      folder.members = entry.layerIds.map((layerId) => ({ layerId }));
  }
  return { folderId: payload.folderId, orderChanged: plan.orderChanged };
}

export function removeLayersFromFolder(doc, { folderId, layerIds }) {
  const folder = findFolder(doc, folderId);
  if (!Array.isArray(layerIds) || !layerIds.length || new Set(layerIds).size !== layerIds.length)
    throw Error('Select distinct Folder Layers');
  const wanted = new Set(layerIds),
    members = new Set(folder.members.map((member) => member.layerId));
  if (layerIds.some((id) => !members.has(id))) throw Error('Choose Layers in this Folder');
  if (doc.layers.some((layer) => wanted.has(layer.id) && layer.locked))
    throw Error('Unlock selected Layers first');
  const remaining = doc.layers.filter((layer) => members.has(layer.id) && !wanted.has(layer.id));
  if (!contiguous(doc, remaining)) throw Error('Move Layers from the edge of the Folder');
  folder.members = remaining.map(({ id }) => ({ layerId: id }));
  return { folderId };
}

export function dissolveLayerFolder(doc, { folderId }) {
  findFolder(doc, folderId);
  doc.editor.layerFolders = doc.editor.layerFolders.filter((folder) => folder.id !== folderId);
  return { folderId };
}

export function pruneLayerFolderMember(doc, layerId) {
  const folder = folderForLayer(doc, layerId);
  if (folder) folder.members = folder.members.filter((member) => member.layerId !== layerId);
}

export function insertDuplicatedLayer(doc, sourceId, index, copy) {
  const folder = folderForLayer(doc, sourceId);
  doc.layers.splice(index, 0, copy);
  if (folder) folder.members.push({ layerId: copy.id });
}

// `to` is the insertion index after removing the source from the paint array.
export function canReorderLayerInFolders(doc, layerId, to) {
  const from = doc.layers.findIndex((layer) => layer.id === layerId);
  if (from < 0) return false;
  const sourceFolder = folderForLayer(doc, layerId);
  const remaining = doc.layers.filter((layer) => layer.id !== layerId);
  if (!Number.isInteger(to) || to < 0 || to > remaining.length) return false;
  if (sourceFolder) {
    const siblings = remaining.flatMap((layer, index) =>
      folderForLayer(doc, layer.id)?.id === sourceFolder.id ? [index] : [],
    );
    return siblings.length ? to >= siblings[0] && to <= siblings.at(-1) + 1 : to === from;
  }
  if (to === 0 || to === remaining.length) return true;
  const left = folderForLayer(doc, remaining[to - 1].id),
    right = folderForLayer(doc, remaining[to].id);
  return !left || !right || left.id !== right.id;
}
