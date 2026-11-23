import { captureEditorTarget, editorTargetMatches } from './live-editor-target.js';

const utf8 = new TextEncoder();

export function createReadLiveTextContent({ readTarget, readText }) {
  return ({ target, layerId, nodeId }) => {
    const current = captureEditorTarget(readTarget());
    if (!editorTargetMatches(current, target)) return { status: 'stale-target' };
    const text = readText(layerId, nodeId);
    if (!editorTargetMatches(readTarget(), current)) return { status: 'stale-target' };
    if (typeof text !== 'string') return { status: 'unsupported-target' };
    if (text.length > 1024) return { status: 'unavailable' };
    const reply = { status: 'ok', target: current, text };
    // The native fixture transport has a 4096-byte frame; never return partial text.
    return utf8.encode(JSON.stringify(reply)).byteLength <= 3500
      ? reply
      : { status: 'unavailable' };
  };
}

// This acceptance boundary requests one existing command. The supplied commit
// owns any font preparation and must check isCurrent before dispatch.
export function createLiveTextContent({ readTarget, isEditBlocked, readText, commit }) {
  return async function setText({ target, layerId, nodeId, text }) {
    if (!editorTargetMatches(readTarget(), target)) return { status: 'stale-target' };
    if (isEditBlocked()) return { status: 'edit-in-progress' };
    if (
      ![layerId, nodeId].every(
        (id) => typeof id === 'string' && id.length > 0 && id.length <= 256,
      ) ||
      typeof text !== 'string' ||
      text.length > 1024
    )
      return { status: 'invalid-arguments' };
    const previous = readText(layerId, nodeId);
    if (typeof previous !== 'string') return { status: 'unsupported-target' };
    if (previous === text) return { status: 'unchanged' };
    const isCurrent = () => editorTargetMatches(readTarget(), target) && !isEditBlocked();
    return commit(
      {
        sessionId: target.sessionId,
        expectedRevision: target.sessionRevision,
        type: 'set-text-content',
        payload: { layerId, nodeId, text },
      },
      isCurrent,
    );
  };
}
