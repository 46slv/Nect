import {
  artworkAlignments,
  artworkDistributions,
  artworkGapDistributions,
} from './artwork-alignment-actions.js';
import { editorTargetMatches } from './live-editor-target.js';

function validArtworkLayerIds(layerIds, minimum = 1) {
  return (
    Array.isArray(layerIds) &&
    layerIds.length >= minimum &&
    layerIds.length <= 100 &&
    [...layerIds].every((id) => typeof id === 'string' && id.length > 0 && id.length <= 256) &&
    new Set(layerIds).size === layerIds.length
  );
}

export function validArtworkTranslation(layerIds, delta) {
  return (
    validArtworkLayerIds(layerIds) &&
    Array.isArray(delta) &&
    delta.length === 2 &&
    Number.isFinite(delta[0]) &&
    Number.isFinite(delta[1])
  );
}

export function validArtworkAlignment(layerIds, action, reference) {
  return (
    (reference === 'selection' || reference === 'artboard') &&
    validArtworkLayerIds(layerIds, reference === 'artboard' ? 1 : 2) &&
    artworkAlignments.some(([id]) => id === action)
  );
}

export function validArtworkDistribution(layerIds, action) {
  return validArtworkLayerIds(layerIds, 3) && artworkDistributions.some(([id]) => id === action);
}

export function validArtworkGapDistribution(layerIds, action, spacing) {
  return (
    (spacing === null || (Number.isFinite(spacing) && Math.abs(spacing) <= 100000)) &&
    validArtworkLayerIds(layerIds, spacing === null ? 3 : 2) &&
    artworkGapDistributions.some(([id]) => id === action)
  );
}

function createArtworkEdit({ readTarget, getOwner, isEditBlocked, commit }, type, payloadFor) {
  return function editArtwork(args) {
    const { target, layerIds } = args;
    const matches = () => editorTargetMatches(readTarget(), target);
    if (!matches()) return { status: 'stale-target' };
    const payload = payloadFor(args);
    if (!payload) return { status: 'invalid-arguments' };
    if (isEditBlocked()) return { status: 'edit-in-progress' };
    const owner = getOwner();
    const layers = owner?.snapshot()?.layers;
    if (!Array.isArray(layers)) return { status: 'unavailable' };
    for (const id of layerIds) {
      const [layer, duplicate] = layers.filter((entry) => entry.id === id);
      if (!layer) return { status: 'unsupported-target' };
      if (duplicate || layer.locked) return { status: 'rejected' };
    }
    if (!matches()) return { status: 'stale-target' };
    const receipt = commit(owner, {
      sessionId: target.sessionId,
      expectedRevision: target.sessionRevision,
      type,
      payload,
    });
    if (!receipt) return { status: matches() ? 'rejected' : 'outcome-unknown' };
    return { status: receipt.changed ? 'changed' : 'unchanged' };
  };
}

export function createLiveArtworkTranslation(host) {
  return createArtworkEdit(host, 'translate-artworks', ({ layerIds, delta }) =>
    validArtworkTranslation(layerIds, delta)
      ? { layerIds: [...layerIds], delta: [...delta] }
      : null,
  );
}

export function createLiveArtworkAlignment(host) {
  return createArtworkEdit(host, 'align-artworks', ({ layerIds, action, reference }) =>
    validArtworkAlignment(layerIds, action, reference)
      ? { layerIds: [...layerIds], action, reference }
      : null,
  );
}

export function createLiveArtworkDistribution(host) {
  return createArtworkEdit(host, 'distribute-artworks', ({ layerIds, action }) =>
    validArtworkDistribution(layerIds, action) ? { layerIds: [...layerIds], action } : null,
  );
}

export function createLiveArtworkGapDistribution(host) {
  return createArtworkEdit(host, 'distribute-artwork-gaps', ({ layerIds, action, spacing }) =>
    validArtworkGapDistribution(layerIds, action, spacing)
      ? { layerIds: [...layerIds], action, ...(spacing === null ? {} : { spacing }) }
      : null,
  );
}
