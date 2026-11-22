import { captureEditorTarget, editorTargetMatches } from './live-editor-target.js';
import { validateLayerFolders } from './layer-folders.js';

const validIndex = (value) => Number.isSafeInteger(value) && value >= 0;
const validId = (value) => typeof value === 'string' && value.length > 0 && value.length <= 256;
export const LIVE_OBJECT_PAGE_SIZE = 32;
// Stay under the 4096-byte native frame, including escaped and non-ASCII IDs.
const encoder = new TextEncoder();
const fitsFrame = (reply) => encoder.encode(JSON.stringify(reply)).byteLength <= 3500;

// A target-bound view of authored objects, not evaluated geometry or the project file.
// Layer indexes only have meaning for the captured target and are not durable IDs.
export function readLiveEditorObjects(
  { readTarget, readDocument },
  { target, layerIndex, offset },
) {
  if ((layerIndex !== null && !validIndex(layerIndex)) || !validIndex(offset))
    return { status: 'invalid-arguments' };
  const current = captureEditorTarget(readTarget());
  if (!editorTargetMatches(current, target)) return { status: 'stale-target' };

  const doc = readDocument();
  if (!Array.isArray(doc?.layers)) return { status: 'unavailable' };
  const layers = doc.layers;
  const layer = layerIndex === null ? null : layers[layerIndex];
  if (layerIndex !== null && !layer) return { status: 'unsupported-target' };
  const source = layer ? layer.nodes : layers;
  if (!Array.isArray(source) || (layer && !validId(layer.id))) return { status: 'unavailable' };
  if (offset > source.length) return { status: 'invalid-arguments' };

  const end = Math.min(source.length, offset + LIVE_OBJECT_PAGE_SIZE);
  const items = [];
  const reply = (next) => ({
    status: 'ok',
    target: current,
    layerId: layer?.id ?? null,
    items,
    nextOffset: next < source.length ? next : null,
  });
  let index = offset;
  for (; index < end; index++) {
    const entry = source[index];
    if (!validId(entry?.id) || (layer && !validId(entry.type))) return { status: 'unavailable' };
    items.push(
      layer
        ? { nodeId: entry.id, type: entry.type }
        : {
            layerId: entry.id,
            layerIndex: index,
            locked: entry.locked === true,
            visible: entry.visible !== false,
          },
    );
    if (!fitsFrame(reply(index + 1))) {
      items.pop();
      if (!items.length) return { status: 'unavailable' };
      break;
    }
  }
  if (!editorTargetMatches(readTarget(), current)) return { status: 'stale-target' };
  const page = reply(index);
  return fitsFrame(page) ? page : { status: 'unavailable' };
}

// Stored flags of one authored layer (not render/effective visibility).
// Keep the existing paged artwork.list response stable.
export function readLiveLayerState({ readTarget, readDocument }, { target, layerId }) {
  if (!validId(layerId)) return { status: 'invalid-arguments' };
  const current = captureEditorTarget(readTarget());
  if (!editorTargetMatches(current, target)) return { status: 'stale-target' };
  const layers = readDocument()?.layers;
  if (!Array.isArray(layers)) return { status: 'unavailable' };
  const [layer, duplicate] = layers.filter((entry) => entry?.id === layerId);
  if (!editorTargetMatches(readTarget(), current)) return { status: 'stale-target' };
  if (!layer) return { status: 'unsupported-target' };
  if (duplicate) return { status: 'unavailable' };
  const reply = {
    status: 'ok',
    target: current,
    layerId,
    visible: layer.visible !== false,
    locked: layer.locked === true,
    guide: layer.guide === true,
    solo: layer.solo === true,
  };
  return fitsFrame(reply) ? reply : { status: 'unavailable' };
}

// Authored parameter wires only; expressions, computed dependencies and
// references to other documents are deliberately outside this projection.
export function readLiveNodeInputWires(
  { readTarget, readDocument },
  { target, layerId, nodeId, offset },
) {
  if (!validId(layerId) || !validId(nodeId) || !validIndex(offset))
    return { status: 'invalid-arguments' };
  const current = captureEditorTarget(readTarget());
  if (!editorTargetMatches(current, target)) return { status: 'stale-target' };
  const layers = readDocument()?.layers;
  if (!Array.isArray(layers)) return { status: 'unavailable' };
  const layer = layers.find((entry) => entry.id === layerId);
  const node = layer?.nodes?.find((entry) => entry.id === nodeId);
  if (!node) return { status: 'unsupported-target' };
  const entries = Object.entries(node.params ?? {}).filter(([, param]) => param?.source != null);
  if (offset > entries.length) return { status: 'invalid-arguments' };
  const items = [];
  const reply = (next) => ({
    status: 'ok',
    target: current,
    layerId,
    nodeId,
    items,
    nextOffset: next < entries.length ? next : null,
  });
  let index = offset;
  for (; index < Math.min(entries.length, offset + LIVE_OBJECT_PAGE_SIZE); index++) {
    const [input, param] = entries[index];
    const sourcePort = param.sourcePort ?? null;
    if (!validId(input) || !validId(param.source) || (sourcePort !== null && !validId(sourcePort)))
      return { status: 'unavailable' };
    items.push({ input, sourceId: param.source, sourcePort });
    if (!fitsFrame(reply(index + 1))) {
      items.pop();
      if (!items.length) return { status: 'unavailable' };
      break;
    }
  }
  if (!editorTargetMatches(readTarget(), current)) return { status: 'stale-target' };
  const page = reply(index);
  return fitsFrame(page) ? page : { status: 'unavailable' };
}

// Expose only folder topology. Each membership is a separate paged row, so a
// folder with many children or escaped IDs cannot fill a native frame.
export function readLiveLayerFolders({ readTarget, readDocument }, { target, offset }) {
  if (!validIndex(offset)) return { status: 'invalid-arguments' };
  const current = captureEditorTarget(readTarget());
  if (!editorTargetMatches(current, target)) return { status: 'stale-target' };
  const doc = readDocument();
  if (!Array.isArray(doc?.layers) || doc.layers.length > 100) return { status: 'unavailable' };
  const folders = doc.editor?.layerFolders ?? [];
  if (!Array.isArray(folders) || folders.length > 100) return { status: 'unavailable' };
  try {
    validateLayerFolders(doc, (folderId) => {
      if (!validId(folderId)) throw Error('Invalid Folder ID');
    });
  } catch {
    return { status: 'unavailable' };
  }

  const source = [];
  for (const folder of folders) {
    const parentFolderId = folder.parentId ?? null;
    if (parentFolderId !== null && !validId(parentFolderId)) return { status: 'unavailable' };
    source.push({ kind: 'folder', folderId: folder.id, parentFolderId });
    for (const { layerId } of folder.members) {
      if (!validId(layerId)) return { status: 'unavailable' };
      source.push({ kind: 'membership', folderId: folder.id, layerId });
    }
  }

  if (offset > source.length) return { status: 'invalid-arguments' };
  const items = [];
  const reply = (next) => ({
    status: 'ok',
    target: current,
    items,
    nextOffset: next < source.length ? next : null,
  });
  let index = offset;
  for (; index < Math.min(source.length, offset + LIVE_OBJECT_PAGE_SIZE); index++) {
    items.push(source[index]);
    if (!fitsFrame(reply(index + 1))) {
      items.pop();
      if (!items.length) return { status: 'unavailable' };
      break;
    }
  }

  if (!editorTargetMatches(readTarget(), current)) return { status: 'stale-target' };
  const page = reply(index);
  return fitsFrame(page) ? page : { status: 'unavailable' };
}
