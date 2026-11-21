import { editorTargetMatches } from './live-editor-target.js';

// The host owns the live dispatcher and its draft checks. This narrow entry
// never names a file, commits another document or invents an editing engine.
export function createLiveBlendDistanceOperation({ readTarget, getOwner, isEditBlocked, commit }) {
  return function setDistance({ target, layerId, nodeId, value }) {
    if (!editorTargetMatches(readTarget(), target)) return { status: 'stale-target' };
    if (isEditBlocked()) return { status: 'edit-in-progress' };
    if (typeof layerId !== 'string' || typeof nodeId !== 'string' || !Number.isFinite(value))
      return { status: 'invalid-arguments' };
    const owner = getOwner();
    const layer = owner.snapshot().layers.find((candidate) => candidate.id === layerId);
    const node = layer?.nodes.find((candidate) => candidate.id === nodeId);
    if (node?.type !== 'Blend' || node.params?.distance?.type !== 'Scalar')
      return { status: 'unsupported-target' };
    const receipt = commit(owner, {
      sessionId: target.sessionId,
      expectedRevision: target.sessionRevision,
      type: 'set-parameter',
      payload: { layerId, nodeId, key: 'distance', value },
    });
    return { status: receipt.changed ? 'changed' : 'unchanged', receipt };
  };
}
