import { editorTargetMatches } from './live-editor-target.js';

// A copy does not change the current file binding, the saved revision, or
// the Undo stack. The host authorizes the concrete writer separately.
export function createLiveDocumentCopy({ readTarget, readDocument, isBlocked, writeCopy }) {
  let pending = false;
  return async function copy(target) {
    if (!editorTargetMatches(readTarget(), target)) return { status: 'stale-target' };
    if (pending) return { status: 'busy' };
    if (isBlocked()) return { status: 'edit-in-progress' };
    pending = true;
    try {
      const serialized = readDocument();
      const written = await writeCopy(serialized);
      return { status: written ? 'accepted-by-writer' : 'not-confirmed' };
    } finally {
      pending = false;
    }
  };
}
