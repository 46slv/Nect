import { blankLayer } from './core.js';
import { pruneLayerFolderMember } from './layer-folders.js';

function assertArtworkDocument(doc) {
  if (!doc || !Array.isArray(doc.layers) || doc.layers.length < 1 || doc.layers.length > 100)
    throw Error('Invalid artwork document');
}

function assertNoSurvivingCompositionConsumers(doc, deletingIds) {
  for (const consumer of doc.layers) {
    if (deletingIds.has(consumer.id)) continue;
    for (const node of consumer.nodes) {
      const targetId = node.compositionReference?.compositionId;
      if (!['Composition Reference', 'SVG'].includes(node.type) || !deletingIds.has(targetId))
        continue;
      const target = doc.layers.find((layer) => layer.id === targetId);
      throw Error(`Cannot delete "${target.name}": used by "${consumer.name}"`);
    }
  }
}

function removeValidatedArtworkLayer(doc, layerId) {
  const target = doc.layers.find((layer) => layer.id === layerId),
    index = doc.layers.indexOf(target),
    replacement = doc.layers.length === 1 ? blankLayer() : null,
    nextLayerId = replacement?.id || doc.layers[index + 1]?.id || doc.layers[index - 1].id;
  pruneLayerFolderMember(doc, layerId);
  doc.layers.splice(index, 1, ...(replacement ? [replacement] : []));
  return { removedLayerId: layerId, nextLayerId, replacedLastLayer: !!replacement };
}

/**
 * Remove exactly one composition and its complete graph. This model-only helper
 * never evaluates artwork or collects assets. Callers own selection and history.
 * The neighbor is the next layer in document order, otherwise the previous one.
 */
export function removeArtworkLayer(doc, layerId) {
  assertArtworkDocument(doc);
  if (typeof layerId !== 'string' || !layerId) throw Error('Choose an existing layer to delete');
  const matches = doc.layers.filter((layer) => layer.id === layerId);
  if (matches.length !== 1) throw Error('Layer is unavailable; select it again');
  const target = matches[0];
  if (target.locked) throw Error('Composition is locked');
  assertNoSurvivingCompositionConsumers(doc, new Set([layerId]));
  return removeValidatedArtworkLayer(doc, layerId);
}

// Validate the complete selection before touching the candidate. The Session
// commits this whole operation once, including the final blank replacement.
export function removeArtworkLayers(doc, layerIds) {
  if (!Array.isArray(layerIds) || !layerIds.length || new Set(layerIds).size !== layerIds.length)
    throw Error('Select distinct artwork layers');
  assertArtworkDocument(doc);
  for (const id of layerIds) {
    const matches = doc.layers.filter((layer) => layer.id === id);
    if (typeof id !== 'string' || matches.length !== 1)
      throw Error('Layer is unavailable; select it again');
    if (matches[0].locked) throw Error('Composition is locked');
  }
  assertNoSurvivingCompositionConsumers(doc, new Set(layerIds));
  const ids = doc.layers.filter((layer) => layerIds.includes(layer.id)).map((layer) => layer.id);
  let result;
  for (const id of ids) result = removeValidatedArtworkLayer(doc, id);
  return { ...result, removedLayerIds: ids };
}
