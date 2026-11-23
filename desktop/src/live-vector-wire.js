import { activeParameters, outputPorts, socketCompatibleFor } from './core.js';
import { editorTargetMatches } from './live-editor-target.js';

const id = (v) => typeof v === 'string' && v.length > 0 && v.length <= 256;

// Only the named operation's literal type is allowed. Keep expression-driven
// inputs and cross-layer references out of this path, even if GUI supports them.
function createLiveTypedWireEdit({ readTarget, getOwner, isEditBlocked, commit }, valueType) {
  return ({ target, layerId, nodeId, input, sourceId, sourcePort }) => {
    const matches = () => editorTargetMatches(readTarget(), target);
    if (!matches()) return { status: 'stale-target' };
    if (
      ![layerId, nodeId, input].every(id) ||
      (sourceId !== null && !id(sourceId)) ||
      (sourcePort !== null && (!id(sourcePort) || sourceId === null))
    )
      return { status: 'invalid-arguments' };
    if (isEditBlocked()) return { status: 'edit-in-progress' };
    const owner = getOwner();
    const layers = owner?.snapshot()?.layers;
    if (!matches()) return { status: 'stale-target' };
    if (!Array.isArray(layers)) return { status: 'unavailable' };
    const [layer, duplicateLayer] = layers.filter((row) => row?.id === layerId);
    if (!layer) return { status: 'unsupported-target' };
    if (duplicateLayer || !Array.isArray(layer.nodes)) return { status: 'unavailable' };
    const [node, duplicateNode] = layer.nodes.filter((row) => row?.id === nodeId);
    if (!node) return { status: 'unsupported-target' };
    if (duplicateNode) return { status: 'unavailable' };
    const stored = Object.hasOwn(node.params ?? {}, input) && node.params[input];
    const entry = activeParameters(node).find(([key]) => key === input);
    if (!stored || entry?.[1] !== stored || stored.type !== valueType)
      return { status: 'unsupported-target' };
    if (layer.locked || node.locked || node.bypassed || stored.locked || stored.expression)
      return { status: 'rejected' };
    if (sourceId !== null) {
      const [source, duplicateSource] = layer.nodes.filter((row) => row?.id === sourceId);
      if (!source) return { status: 'unsupported-target' };
      if (duplicateSource) return { status: 'unavailable' };
      const port = sourcePort ?? undefined;
      if (
        source.id === nodeId ||
        !outputPorts(source).some(({ id, type }) => id === port && type === valueType) ||
        !socketCompatibleFor(source, node, input, port)
      )
        return { status: 'rejected' };
    }
    if (!matches()) return { status: 'stale-target' };
    if ((stored.source ?? null) === sourceId && (stored.sourcePort ?? null) === sourcePort)
      return { status: 'unchanged' };
    const payload = { layerId, nodeId, key: input };
    if (sourceId !== null) {
      payload.sourceId = sourceId;
      if (sourcePort !== null) payload.sourcePort = sourcePort;
    }
    const receipt = commit(owner, {
      sessionId: target.sessionId,
      expectedRevision: target.sessionRevision,
      type: sourceId === null ? 'disconnect' : 'connect',
      payload,
    });
    if (!receipt) return { status: matches() ? 'rejected' : 'outcome-unknown' };
    return { status: receipt.changed ? 'changed' : 'unchanged' };
  };
}

export const createLiveVectorWireEdit = (host) => createLiveTypedWireEdit(host, 'Vector2');
export const createLiveScalarWireEdit = (host) => createLiveTypedWireEdit(host, 'Scalar');
