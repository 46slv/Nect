import { editorTargetMatches } from './live-editor-target.js';

// GUI eye/lock buttons and explicit external values use the same Session
// command. Never accept arbitrary layer-state keys or an implicit toggle.
function createLiveLayerFlag(key, { readTarget, getOwner, isEditBlocked, commit }) {
  return function setFlag({ target, layerId, [key]: value }) {
    if (!editorTargetMatches(readTarget(), target)) return { status: 'stale-target' };
    if (
      typeof layerId !== 'string' ||
      !layerId.length ||
      layerId.length > 256 ||
      typeof value !== 'boolean'
    )
      return { status: 'invalid-arguments' };
    if (isEditBlocked()) return { status: 'edit-in-progress' };
    const owner = getOwner();
    const layer = owner.snapshot().layers.find((entry) => entry.id === layerId);
    if (!layer || typeof layer[key] !== 'boolean') return { status: 'unsupported-target' };
    if (layer[key] === value) return { status: 'unchanged' };
    const receipt = commit(owner, {
      sessionId: target.sessionId,
      expectedRevision: target.sessionRevision,
      type: 'set-layer-state',
      payload: { layerId, key, value },
    });
    if (!receipt) return { status: 'rejected' };
    return { status: receipt.changed ? 'changed' : 'unchanged' };
  };
}
export function createLiveLayerVisibility(host) {
  return createLiveLayerFlag('visible', host);
}

export function createLiveLayerLock(host) {
  return createLiveLayerFlag('locked', host);
}

export function createLiveLayerRename({ readTarget, getOwner, isEditBlocked, commit }) {
  return function renameLayer({ target, layerId, name }) {
    if (!editorTargetMatches(readTarget(), target)) return { status: 'stale-target' };
    if (
      typeof layerId !== 'string' ||
      !layerId.length ||
      layerId.length > 256 ||
      typeof name !== 'string' ||
      name.length > 100000 ||
      !name.trim()
    )
      return { status: 'invalid-arguments' };
    if (isEditBlocked()) return { status: 'edit-in-progress' };
    const owner = getOwner();
    const layer = owner.snapshot().layers.find((entry) => entry.id === layerId);
    if (!layer) return { status: 'unsupported-target' };
    if (layer.locked) return { status: 'rejected' };
    const value = name.trim();
    if (layer.name === value) return { status: 'unchanged' };
    const receipt = commit(owner, {
      sessionId: target.sessionId,
      expectedRevision: target.sessionRevision,
      type: 'rename',
      payload: { layerId, name: value, expectedName: layer.name },
    });
    if (!receipt) return { status: 'rejected' };
    return { status: receipt.changed ? 'changed' : 'unchanged' };
  };
}
