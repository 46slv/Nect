import { ARTWORK_TYPES } from './artwork-creation.js';
import { editorTargetMatches } from './live-editor-target.js';

export function createLiveArtworkCreation({ readTarget, isEditBlocked, commit }) {
  return function create({ target, artworkType, at }) {
    if (!editorTargetMatches(readTarget(), target)) return { status: 'stale-target' };
    if (isEditBlocked()) return { status: 'edit-in-progress' };
    if (
      !ARTWORK_TYPES.includes(artworkType) ||
      !Array.isArray(at) ||
      at.length !== 2 ||
      ![at[0], at[1]].every((value) => Number.isFinite(value) && Math.abs(value) <= 1_000_000)
    )
      return { status: 'invalid-arguments' };
    const { layerId, nodeId } = commit({
      sessionId: target.sessionId,
      expectedRevision: target.sessionRevision,
      type: 'create-artwork',
      payload: { artworkType, at: [...at] },
    });
    return { status: 'changed', layerId, nodeId };
  };
}
