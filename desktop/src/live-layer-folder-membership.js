import { planLayerFolderRemoval } from './layer-folders.js';
import { editorTargetMatches } from './live-editor-target.js';

export function createLiveLayerFolderMembership({ readTarget, getOwner, isEditBlocked, commit }) {
  return function changeMembership({ target, folderId, layerIds, mode }) {
    if (!editorTargetMatches(readTarget(), target)) return { status: 'stale-target' };
    if (isEditBlocked()) return { status: 'edit-in-progress' };
    const owner = getOwner();
    const doc = owner.snapshot();
    if (!doc.editor?.layerFolders?.some((folder) => folder.id === folderId))
      return { status: 'unsupported-target' };
    const payload = { folderId, layerIds };
    if (mode === 'remove') {
      let plan;
      try {
        plan = planLayerFolderRemoval(doc, payload);
      } catch {
        return { status: 'rejected' };
      }
      if (plan.orderChanged) return { status: 'requires-order-review' };
      payload.expectedPaintOrder = plan.paintOrder;
    }
    const receipt = commit(owner, {
      sessionId: target.sessionId,
      expectedRevision: target.sessionRevision,
      type: mode === 'add' ? 'add-layers-to-folder' : 'remove-layers-from-folder',
      payload,
    });
    if (!receipt?.changed)
      return { status: editorTargetMatches(readTarget(), target) ? 'rejected' : 'outcome-unknown' };
    return { status: receipt.folderId === folderId ? 'changed' : 'outcome-unknown' };
  };
}
