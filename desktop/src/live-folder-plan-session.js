import { editorTargetMatches } from './live-editor-target.js';
import { readLiveLayerFolderPlan } from './live-layer-folder-plan.js';

const commands = Object.freeze({
  move: 'move-layers-to-folder',
  remove: 'remove-layers-from-folder',
  order: 'order-layer-folder',
});

// One reviewed plan per adapter connection; never a grant or an authorization token.
export function createLiveFolderPlanSession({
  readTarget,
  readDocument,
  getOwner,
  isEditBlocked,
  commit,
}) {
  let pending = null;
  return {
    preview({ target, request }) {
      pending = null;
      const reply = readLiveLayerFolderPlan({ readTarget, readDocument }, { target, request });
      if (reply.status !== 'ok') return reply;
      const planId = crypto.randomUUID();
      pending = {
        planId,
        target: { ...target },
        request: structuredClone(request),
        evidence: JSON.stringify(reply),
      };
      return { ...reply, planId };
    },
    apply({ target, planId }) {
      if (!pending || pending.planId !== planId) return { status: 'unknown-plan' };
      const prepared = pending;
      pending = null;
      const matches = () => {
        const current = readTarget();
        return !!current && editorTargetMatches(current, target);
      };
      if (!matches() || !editorTargetMatches(target, prepared.target))
        return { status: 'stale-target' };
      if (isEditBlocked()) return { status: 'edit-in-progress' };
      const current = readLiveLayerFolderPlan(
        { readTarget, readDocument },
        { target: prepared.target, request: prepared.request },
      );
      if (current.status !== 'ok') return { status: current.status };
      if (JSON.stringify(current) !== prepared.evidence) return { status: 'plan-conflict' };
      const { mode, ...payload } = prepared.request;
      const owner = getOwner();
      if (!matches()) return { status: 'stale-target' };
      const receipt = commit(owner, {
        sessionId: target.sessionId,
        expectedRevision: target.sessionRevision,
        type: commands[mode],
        payload: { ...payload, expectedPaintOrder: current.after },
      });
      if (!receipt?.changed)
        return { status: matches() ? (receipt ? 'unchanged' : 'rejected') : 'outcome-unknown' };
      return { status: receipt.folderId === payload.folderId ? 'changed' : 'outcome-unknown' };
    },
    revoke() {
      pending = null;
    },
  };
}
