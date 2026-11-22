import { editorTargetMatches } from './live-editor-target.js';

export function createLiveLayerDeletion({ readTarget, getOwner, isEditBlocked, commit }) {
  return function deleteLayer({ target, layerId, confirmed }) {
    const matchesTarget = () => editorTargetMatches(readTarget(), target);
    if (!matchesTarget()) return { status: 'stale-target' };
    if (
      confirmed !== true ||
      typeof layerId !== 'string' ||
      !layerId.length ||
      layerId.length > 256
    )
      return { status: 'invalid-arguments' };
    if (isEditBlocked()) return { status: 'edit-in-progress' };
    const owner = getOwner();
    const existing = owner?.snapshot()?.layers;
    if (!Array.isArray(existing)) return { status: 'unavailable' };
    const [layer, duplicate] = existing.filter((entry) => entry.id === layerId);
    if (!layer) return { status: 'unsupported-target' };
    if (duplicate || layer.locked) return { status: 'rejected' };
    if (!matchesTarget()) return { status: 'stale-target' };
    const receipt = commit(
      owner,
      { sessionId: target.sessionId, expectedRevision: target.sessionRevision },
      layerId,
    );
    if (!receipt?.changed) return { status: matchesTarget() ? 'rejected' : 'outcome-unknown' };
    if (
      receipt.removedLayerId !== layerId ||
      typeof receipt.nextLayerId !== 'string' ||
      !receipt.nextLayerId ||
      typeof receipt.replacedLastLayer !== 'boolean'
    )
      return { status: 'outcome-unknown' };
    return {
      status: 'changed',
      removedLayerId: layerId,
      nextLayerId: receipt.nextLayerId,
      replacedLastLayer: receipt.replacedLastLayer,
    };
  };
}
