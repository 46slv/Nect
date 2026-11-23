import { editorTargetMatches } from './live-editor-target.js';

// Copy an authored layer in the current document only. The existing Session
// owns resource checks, ID remapping, atomic validation and history.
export function createLiveLayerDuplicate({ readTarget, getOwner, isEditBlocked, commit }) {
  return function duplicateLayer({ target, layerId }) {
    if (!editorTargetMatches(readTarget(), target)) return { status: 'stale-target' };
    if (typeof layerId !== 'string' || !layerId.length || layerId.length > 256)
      return { status: 'invalid-arguments' };
    if (isEditBlocked()) return { status: 'edit-in-progress' };

    const owner = getOwner();
    const doc = owner.snapshot();
    const matches = doc.layers.filter((entry) => entry.id === layerId);
    if (matches.length !== 1) return { status: 'unsupported-target' };
    if (matches[0].locked || doc.layers.length >= 100) return { status: 'rejected' };

    const receipt = commit(owner, {
      sessionId: target.sessionId,
      expectedRevision: target.sessionRevision,
      type: 'duplicate-layer',
      payload: { layerId },
    });
    if (!receipt?.changed) return { status: 'rejected' };
    if (
      receipt.sourceLayerId !== layerId ||
      typeof receipt.layerId !== 'string' ||
      !receipt.layerId ||
      receipt.layerId === layerId
    )
      return { status: 'outcome-unknown' };
    return { status: 'changed', sourceLayerId: layerId, layerId: receipt.layerId };
  };
}
