import { editorTargetMatches } from './live-editor-target.js';

// Opt-in route over the same transactional Session command as the GUI.
export function createLiveLayerFolder({ readTarget, getOwner, isEditBlocked, commit }) {
  return function createFolder({ target, name, layerIds, folderIds }) {
    if (!editorTargetMatches(readTarget(), target)) return { status: 'stale-target' };
    if (isEditBlocked()) return { status: 'edit-in-progress' };
    const owner = getOwner();
    const receipt = commit(owner, {
      sessionId: target.sessionId,
      expectedRevision: target.sessionRevision,
      type: 'create-layer-folder',
      payload: { name, layerIds, folderIds },
    });
    if (!receipt?.changed)
      return { status: editorTargetMatches(readTarget(), target) ? 'rejected' : 'outcome-unknown' };
    if (typeof receipt.folderId !== 'string' || !receipt.folderId.length)
      return { status: 'outcome-unknown' };
    return { status: 'changed', folderId: receipt.folderId };
  };
}
