import { editorTargetMatches } from './live-editor-target.js';
import { blendNodeDeclaration } from './blend-node-declaration.js';

// The host owns the live dispatcher and its draft checks. This narrow entry
// never names a file, commits another document or invents an editing engine.
function createBlendParameterOperation({ readTarget, getOwner, isEditBlocked, commit }, key) {
  return function setDistance({ target, layerId, nodeId, value }) {
    if (!editorTargetMatches(readTarget(), target)) return { status: 'stale-target' };
    if (isEditBlocked()) return { status: 'edit-in-progress' };
    if (
      typeof layerId !== 'string' ||
      typeof nodeId !== 'string' ||
      !Number.isFinite(value) ||
      (key === 'steps' && (!Number.isInteger(value) || value < 0 || value > 254))
    )
      return { status: 'invalid-arguments' };
    const owner = getOwner();
    const layer = owner.snapshot().layers.find((candidate) => candidate.id === layerId);
    const node = layer?.nodes.find((candidate) => candidate.id === nodeId);
    if (node?.type !== 'Blend' || node.params?.[key]?.type !== 'Scalar')
      return { status: 'unsupported-target' };
    if (key === 'steps') {
      const operation =
        blendNodeDeclaration.operationAliases[node.operation] ??
        (node.operation || blendNodeDeclaration.defaultOperation);
      const enabled = blendNodeDeclaration.operations.some(
        (row) => row.id === operation && row.active.includes(key),
      );
      const parameter = node.params[key];
      if (
        !enabled ||
        layer.locked ||
        node.locked ||
        node.bypassed ||
        parameter.locked ||
        parameter.source ||
        parameter.expression
      )
        return { status: 'unsupported-target' };
      if (operation === 'three-source-steps' && value > 126) return { status: 'invalid-arguments' };
    }
    const receipt = commit(owner, {
      sessionId: target.sessionId,
      expectedRevision: target.sessionRevision,
      type: 'set-parameter',
      payload: { layerId, nodeId, key, value },
    });
    if (!receipt) return { status: 'rejected' };
    return { status: receipt.changed ? 'changed' : 'unchanged', receipt };
  };
}

export function createLiveBlendDistanceOperation(host) {
  return createBlendParameterOperation(host, 'distance');
}

export function createLiveBlendStepsOperation(host) {
  return createBlendParameterOperation(host, 'steps');
}
