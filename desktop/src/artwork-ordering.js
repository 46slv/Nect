const unavailable = (code, reason) => ({ supported: false, code, reason });

/** Resolve a one-position paint-order change by exact layer ID, without evaluation. */
export function resolveArtworkOrderTarget(doc, layerId, direction) {
  if (!doc || !Array.isArray(doc.layers) || doc.layers.length < 1 || doc.layers.length > 100)
    return unavailable('invalid-document', 'Choose a layer in a valid document.');
  if (direction !== 'forward' && direction !== 'backward')
    return unavailable('invalid-direction', 'Choose Forward or Backward.');
  if (typeof layerId !== 'string' || !layerId)
    return unavailable('missing-layer', 'Choose an existing layer.');
  const matches = doc.layers.filter((layer) => layer?.id === layerId);
  if (matches.length !== 1)
    return unavailable('unavailable-layer', 'Layer is unavailable; select it again.');
  const target = matches[0],
    index = doc.layers.indexOf(target),
    nextIndex = index + (direction === 'forward' ? 1 : -1);
  if (target.locked) return unavailable('locked-layer', 'Unlock this layer before ordering it.');
  if (nextIndex < 0 || nextIndex >= doc.layers.length)
    return unavailable(
      'boundary',
      direction === 'forward' ? 'Already the front layer.' : 'Already the back layer.',
    );
  if (!canReorderLayerInFolders(doc, layerId, nextIndex))
    return unavailable('folder-boundary', 'Move Layers into or out of a Folder explicitly.');
  return { supported: true, index, nextIndex };
}

/**
 * Permute only the existing layer array. Every layer/graph/asset record keeps its
 * identity and bytes; crossing a locked neighbor does not edit that record.
 * Callers own selection/history. A boundary is an explicit unchanged result.
 */
export function moveArtworkLayer(doc, layerId, direction) {
  const target = resolveArtworkOrderTarget(doc, layerId, direction);
  if (!target.supported) {
    if (target.code === 'boundary') return { moved: false, layerId, direction };
    throw Error(target.reason);
  }
  const { index, nextIndex } = target;
  [doc.layers[index], doc.layers[nextIndex]] = [doc.layers[nextIndex], doc.layers[index]];
  return { moved: true, layerId, direction };
}

/** Resolve an exact insertion without editing layers or folder membership. */
export function resolveArtworkReorderTarget(doc, layerId, beforeLayerId) {
  const source = doc?.layers?.find((layer) => layer.id === layerId);
  const before = doc?.layers?.find((layer) => layer.id === beforeLayerId);
  if (!source || (beforeLayerId !== null && !before) || source === before) return { moved: false };
  if (source.locked) return { error: 'Unlock this layer before ordering it.' };
  const from = doc.layers.indexOf(source);
  let to = beforeLayerId === null ? doc.layers.length : doc.layers.indexOf(before);
  if (from < to) to -= 1;
  if (!canReorderLayerInFolders(doc, layerId, to))
    return { error: 'Move Layers into or out of a Folder explicitly.' };
  return { moved: from !== to, from, to, source };
}

/** Move one layer to the exact visual insertion point without changing IDs. */
export function moveArtworkLayerBefore(doc, layerId, beforeLayerId) {
  const target = resolveArtworkReorderTarget(doc, layerId, beforeLayerId);
  if (target.error) throw Error(target.error);
  if (!target.source) return target;
  if (target.moved) {
    doc.layers.splice(target.from, 1);
    doc.layers.splice(target.to, 0, target.source);
  }
  return { moved: target.moved, layerId, beforeLayerId };
}
import { canReorderLayerInFolders } from './layer-folders.js';
