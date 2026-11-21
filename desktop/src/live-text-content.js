import { editorTargetMatches } from './live-editor-target.js';

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
