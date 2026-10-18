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

/** Move one layer to the exact visual insertion point without changing IDs. */
export function moveArtworkLayerBefore(doc, layerId, beforeLayerId) {
  const source = doc?.layers?.find((layer) => layer.id === layerId);
  const before = doc?.layers?.find((layer) => layer.id === beforeLayerId);
  if (!source || !before || source === before) return { moved: false };
  if (source.locked) throw Error('Unlock this layer before ordering it.');
  const from = doc.layers.indexOf(source);
  let to = doc.layers.indexOf(before);
  doc.layers.splice(from, 1);
  if (from < to) to -= 1;
  doc.layers.splice(to, 0, source);
  return { moved: from !== to, layerId, beforeLayerId };
}
