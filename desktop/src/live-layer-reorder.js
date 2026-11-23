import { resolveArtworkReorderTarget } from './artwork-ordering.js';
import { editorTargetMatches } from './live-editor-target.js';

const id = (value) => typeof value === 'string' && value.length > 0 && value.length <= 256;

// The transport is deliberately separate. Opting into this handler never
// enables a public transport capability or bypasses the owning Session.
export function createLiveLayerReorder({ readTarget, getOwner, isEditBlocked, commit }) {
  return function reorderLayer({ target, layerId, beforeLayerId }) {
    if (!editorTargetMatches(readTarget(), target)) return { status: 'stale-target' };
    if (!id(layerId) || (beforeLayerId !== null && !id(beforeLayerId)))
      return { status: 'invalid-arguments' };
    if (isEditBlocked()) return { status: 'edit-in-progress' };
    const owner = getOwner();
    const document = owner.snapshot();
    const matchedOnce = (layerId) =>
      document.layers.filter((entry) => entry.id === layerId).length === 1;
    if (!matchedOnce(layerId) || (beforeLayerId !== null && !matchedOnce(beforeLayerId)))
      return { status: 'unsupported-target' };
    const resolved = resolveArtworkReorderTarget(document, layerId, beforeLayerId);
    if (resolved.error) return { status: 'rejected' };
    if (!resolved.moved) return { status: 'unchanged' };

    const receipt = commit(owner, {
      sessionId: target.sessionId,
      expectedRevision: target.sessionRevision,
      type: 'reorder-layer',
      payload: { layerId, beforeLayerId },
    });
    if (!receipt) return { status: 'rejected' };
    return { status: receipt.changed ? 'changed' : 'unchanged' };
  };
}
