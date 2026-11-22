import { editorTargetMatches } from './live-editor-target.js';

export function validArtworkTranslation(layerIds, delta) {
  return (
    Array.isArray(layerIds) &&
    layerIds.length > 0 &&
    layerIds.length <= 100 &&
    [...layerIds].every((id) => typeof id === 'string' && id.length > 0 && id.length <= 256) &&
    new Set(layerIds).size === layerIds.length &&
    Array.isArray(delta) &&
    delta.length === 2 &&
    Number.isFinite(delta[0]) &&
    Number.isFinite(delta[1])
  );
}

export function createLiveArtworkTranslation({ readTarget, getOwner, isEditBlocked, commit }) {
  return function translateArtwork({ target, layerIds, delta }) {
    const matches = () => editorTargetMatches(readTarget(), target);
    if (!matches()) return { status: 'stale-target' };
    if (!validArtworkTranslation(layerIds, delta)) return { status: 'invalid-arguments' };
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
      type: 'translate-artworks',
      payload: { layerIds: [...layerIds], delta: [...delta] },
    });
    if (!receipt) return { status: matches() ? 'rejected' : 'outcome-unknown' };
    return { status: receipt.changed ? 'changed' : 'unchanged' };
  };
}
