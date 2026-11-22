import { editorTargetMatches } from './live-editor-target.js';

export const isSolidFillHex = (value) => typeof value === 'string' && /^#[0-9a-f]{6}$/i.test(value);

export function createLiveSolidFillColor({ readTarget, getOwner, isEditBlocked, commit }) {
  return ({ target, layerId, nodeId, hex }) => {
    const matches = () => editorTargetMatches(readTarget(), target);
    if (!matches()) return { status: 'stale-target' };
    if (!isSolidFillHex(hex)) return { status: 'invalid-arguments' };
    if (isEditBlocked()) return { status: 'edit-in-progress' };
    const owner = getOwner();
    const layers = owner?.snapshot()?.layers;
    if (!Array.isArray(layers)) return { status: 'unavailable' };
    const foundLayers = layers.filter((layer) => layer.id === layerId);
    if (foundLayers.length !== 1) return { status: 'unsupported-target' };
    const [layer] = foundLayers;
    const foundNodes = layer.nodes.filter((node) => node.id === nodeId);
    if (foundNodes.length !== 1 || foundNodes[0].type !== 'Fill')
      return { status: 'unsupported-target' };
    const [node] = foundNodes;
    const color = node.params.color;
    if (!color || color.type !== 'Color') return { status: 'unsupported-target' };
    if (
      layer.locked ||
      node.locked ||
      node.bypassed ||
      color.locked ||
      color.expression ||
      color.colorSlotId !== undefined ||
      color.source ||
      !isSolidFillHex(color.value?.hex) ||
      !Number.isFinite(color.value?.alpha)
    )
      return { status: 'rejected' };
    if (!matches()) return { status: 'stale-target' };
    if (color.value.hex.toLowerCase() === hex.toLowerCase()) return { status: 'unchanged' };
    const receipt = commit(owner, {
      sessionId: target.sessionId,
      expectedRevision: target.sessionRevision,
      type: 'set-parameter',
      payload: { layerId, nodeId, key: 'color', value: { ...color.value, hex } },
    });
    if (!receipt) return { status: matches() ? 'rejected' : 'outcome-unknown' };
    return { status: receipt.changed ? 'changed' : 'unchanged' };
  };
}
