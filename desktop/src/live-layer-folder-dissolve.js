import { editorTargetMatches } from './live-editor-target.js';

export function createLiveLayerFolderDissolve({ readTarget, getOwner, isEditBlocked, commit }) {
  return function dissolveFolder({ target, folderId }) {
    if (!editorTargetMatches(readTarget(), target)) return { status: 'stale-target' };
    if (isEditBlocked()) return { status: 'edit-in-progress' };
    const owner = getOwner();
    if (!owner.snapshot().editor?.layerFolders?.some((entry) => entry.id === folderId))
      return { status: 'unsupported-target' };
    // The shared command removes only folder metadata, promotes children and keeps painting order.
    const receipt = commit(owner, {
      sessionId: target.sessionId,
      expectedRevision: target.sessionRevision,
      type: 'dissolve-layer-folder',
      payload: { folderId },
    });
    if (!receipt?.changed)
      return { status: editorTargetMatches(readTarget(), target) ? 'rejected' : 'outcome-unknown' };
    return { status: receipt.folderId === folderId ? 'changed' : 'outcome-unknown' };
  };
}
