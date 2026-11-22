import { editorTargetMatches } from './live-editor-target.js';
import { blendNodeDeclaration } from './blend-node-declaration.js';

function blendOperation(node) {
  const { operationAliases, defaultOperation, operations } = blendNodeDeclaration;
  const raw = node.operation || defaultOperation;
  const id = Object.hasOwn(operationAliases, raw) ? operationAliases[raw] : raw;
  return operations.find((op) => op.id === id);
}

function addressedBlend(owner, layerId, nodeId) {
  const layer = owner.snapshot().layers.find((row) => row.id === layerId);
  const node = layer?.nodes.find((row) => row.id === nodeId);
  return node?.type === 'Blend' ? { layer, node } : {};
}

export function createReadLiveBlendSettings({ readTarget, getOwner }) {
  return function read({ target, layerId, nodeId }) {
    if (!editorTargetMatches(readTarget(), target)) return { status: 'stale-target' };
    const { layer, node } = addressedBlend(getOwner(), layerId, nodeId);
    const operation = node && blendOperation(node);
    if (!operation) return { status: 'unsupported-target' };
    const key = operation.active.find((id) => id === 'distance' || id === 'steps');
    if (!key) return { status: 'ok', mode: operation.id, spacing: null };
    const param = node.params?.[key];
    if (param?.type !== 'Scalar') return { status: 'unsupported-target' };
    const control = param.source ? 'connection' : param.expression ? 'expression' : 'literal';
    return {
      status: 'ok',
      mode: operation.id,
      spacing: {
        kind: key,
        control,
        value: control === 'literal' && Number.isFinite(param.value) ? param.value : null,
        editable:
          control === 'literal' && !layer.locked && !node.locked && !node.bypassed && !param.locked,
        maxSteps: key === 'steps' ? (operation.id === 'three-source-steps' ? 126 : 254) : null,
      },
    };
  };
}

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
    const { layer, node } = addressedBlend(owner, layerId, nodeId);
    if (node?.params?.[key]?.type !== 'Scalar') return { status: 'unsupported-target' };
    if (key === 'steps') {
      const operation = blendOperation(node);
      const enabled = operation?.active.includes(key);
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
      if (operation.id === 'three-source-steps' && value > 126)
        return { status: 'invalid-arguments' };
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
