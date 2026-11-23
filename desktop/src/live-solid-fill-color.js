import { captureEditorTarget, editorTargetMatches } from './live-editor-target.js';

export const isSolidFillHex = (value) => typeof value === 'string' && /^#[0-9a-f]{6}$/i.test(value);

export const isSolidFillAlpha = (value) => Number.isFinite(value) && value >= 0 && value <= 1;

function exactFill(layers, layerId, nodeId) {
  if (!Array.isArray(layers)) return { status: 'unavailable' };
  const [layer, duplicateLayer] = layers.filter((entry) => entry.id === layerId);
  if (!layer || duplicateLayer || !Array.isArray(layer.nodes))
    return { status: 'unsupported-target' };
  const [node, duplicateNode] = layer.nodes.filter((entry) => entry.id === nodeId);
  const color = node?.params?.color;
  if (!node || duplicateNode || node.type !== 'Fill' || color?.type !== 'Color')
    return { status: 'unsupported-target' };
  return { layer, node, color };
}

const isControlled = (color) =>
  Boolean(color.expression || color.colorSlotId !== undefined || color.source);
const hasLiteral = (color) =>
  isSolidFillHex(color.value?.hex) && Number.isFinite(color.value?.alpha);
const isLocked = ({ layer, node, color }) => Boolean(layer.locked || node.locked || color.locked);

// Authored literal only; never expose an expression, source, or resolved color.
export function readLiveSolidFillColor({ readTarget, readDocument }, { target, layerId, nodeId }) {
  const current = captureEditorTarget(readTarget());
  if (!editorTargetMatches(current, target)) return { status: 'stale-target' };
  const fill = exactFill(readDocument()?.layers, layerId, nodeId);
  if (!editorTargetMatches(readTarget(), current)) return { status: 'stale-target' };
  if (fill.status) return { status: fill.status };
  const { node, color } = fill;
  const mode = isControlled(color) ? 'controlled' : hasLiteral(color) ? 'literal' : 'unavailable';
  const locked = isLocked(fill),
    bypassed = node.bypassed === true;
  return {
    status: 'ok',
    target: current,
    mode,
    rgb: mode === 'literal' ? color.value.hex.toLowerCase() : null,
    alpha: mode === 'literal' ? color.value.alpha : null,
    locked,
    bypassed,
    editable: mode === 'literal' && !locked && !bypassed,
  };
}

export const createLiveSolidFillColor = (host) =>
  createLiveSolidFillEdit(host, 'hex', isSolidFillHex, (v) => v.toLowerCase());
export const createLiveSolidFillOpacity = (host) =>
  createLiveSolidFillEdit(host, 'alpha', isSolidFillAlpha, (v) => v);

function createLiveSolidFillEdit(
  { readTarget, getOwner, isEditBlocked, commit },
  field,
  valid,
  compare,
) {
  return ({ target, layerId, nodeId, [field]: value }) => {
    const matches = () => editorTargetMatches(readTarget(), target);
    if (!matches()) return { status: 'stale-target' };
    if (!valid(value)) return { status: 'invalid-arguments' };
    if (isEditBlocked()) return { status: 'edit-in-progress' };
    const owner = getOwner();
    const fill = exactFill(owner?.snapshot()?.layers, layerId, nodeId);
    if (fill.status) return { status: fill.status };
    const { node, color } = fill;
    if (isLocked(fill) || node.bypassed || isControlled(color) || !hasLiteral(color))
      return { status: 'rejected' };
    if (!matches()) return { status: 'stale-target' };
    if (compare(color.value[field]) === compare(value)) return { status: 'unchanged' };
    const receipt = commit(owner, {
      sessionId: target.sessionId,
      expectedRevision: target.sessionRevision,
      type: 'set-parameter',
      payload: { layerId, nodeId, key: 'color', value: { ...color.value, [field]: value } },
    });
    if (!receipt) return { status: matches() ? 'rejected' : 'outcome-unknown' };
    return { status: receipt.changed ? 'changed' : 'unchanged' };
  };
}
