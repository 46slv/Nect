import { editorTargetMatches } from './live-editor-target.js';

// The GUI eye button and an explicit external value use the same Session
// command. Never accept arbitrary layer-state keys or an implicit toggle.
export function createLiveLayerVisibility({ readTarget, getOwner, isEditBlocked, commit }) {
  return function setVisibility({ target, layerId, visible }) {
    if (!editorTargetMatches(readTarget(), target)) return { status: 'stale-target' };
    if (
      typeof layerId !== 'string' ||
      !layerId.length ||
      layerId.length > 256 ||
      typeof visible !== 'boolean'
    )
      return { status: 'invalid-arguments' };
    if (isEditBlocked()) return { status: 'edit-in-progress' };
    const owner = getOwner();
    const layer = owner.snapshot().layers.find((entry) => entry.id === layerId);
    if (!layer || typeof layer.visible !== 'boolean') return { status: 'unsupported-target' };
    if (layer.visible === visible) return { status: 'unchanged' };
    const receipt = commit(owner, {
      sessionId: target.sessionId,
      expectedRevision: target.sessionRevision,
      type: 'set-layer-state',
      payload: { layerId, key: 'visible', value: visible },
    });
    if (!receipt) return { status: 'rejected' };
    return { status: receipt.changed ? 'changed' : 'unchanged' };
  };
}
