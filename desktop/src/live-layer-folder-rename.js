import { editorTargetMatches } from './live-editor-target.js';

export function createLiveLayerFolderRename({ readTarget, getOwner, isEditBlocked, commit }) {
  return function renameFolder({ target, folderId, expectedName, name }) {
    if (!editorTargetMatches(readTarget(), target)) return { status: 'stale-target' };
    if (isEditBlocked()) return { status: 'edit-in-progress' };
    const owner = getOwner();
    const folder = owner.snapshot().editor?.layerFolders?.find((item) => item.id === folderId);
    if (!folder) return { status: 'unsupported-target' };
    if (folder.name !== expectedName) return { status: 'name-conflict' };
    if (folder.name === name.trim()) return { status: 'unchanged' };

    const receipt = commit(owner, {
      sessionId: target.sessionId,
      expectedRevision: target.sessionRevision,
      type: 'rename-layer-folder',
      payload: { folderId, name, expectedName },
    });
    if (!receipt?.changed)
      return { status: editorTargetMatches(readTarget(), target) ? 'rejected' : 'outcome-unknown' };
    return { status: receipt.folderId === folderId ? 'changed' : 'outcome-unknown' };
  };
}
