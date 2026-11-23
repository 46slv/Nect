import { editorTargetMatches } from './live-editor-target.js';

export const validBlankLayerName = (name) =>
  name === null || (typeof name === 'string' && name.length <= 120 && !!name.trim());

export function createLiveBlankLayer({ readTarget, getOwner, isEditBlocked, commit }) {
  return function createBlankLayer({ target, name }) {
    if (!editorTargetMatches(readTarget(), target)) return { status: 'stale-target' };
    if (!validBlankLayerName(name)) return { status: 'invalid-arguments' };
    if (isEditBlocked()) return { status: 'edit-in-progress' };
    const owner = getOwner();
    const existing = owner?.snapshot()?.layers;
    if (!Array.isArray(existing)) return { status: 'unavailable' };
    if (existing.length >= 100) return { status: 'rejected' };
    const ids = new Set(existing.map(({ id }) => id));
    const receipt = commit(owner, {
      sessionId: target.sessionId,
      expectedRevision: target.sessionRevision,
      type: 'create-blank-layer',
      payload: name === null ? {} : { name: name.trim() },
    });
    if (!receipt?.changed)
      return { status: editorTargetMatches(readTarget(), target) ? 'rejected' : 'outcome-unknown' };
    if (
      typeof receipt.layerId !== 'string' ||
      !receipt.layerId ||
      ids.has(receipt.layerId) ||
      typeof receipt.nodeId !== 'string' ||
      !receipt.nodeId
    )
      return { status: 'outcome-unknown' };
    return { status: 'changed', layerId: receipt.layerId, nodeId: receipt.nodeId };
  };
}
