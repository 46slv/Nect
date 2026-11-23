import { editorTargetMatches } from './live-editor-target.js';

// Reopening this acceptance copy is explicit. Only the exact current snapshot
// may replace the editor, and its identity is checked again after reading disk.
export function createLiveDocumentReopen({
  readTarget,
  readDocument,
  isBlocked,
  readCopy,
  loadCopy,
}) {
  return async function reopen(target) {
    if (!editorTargetMatches(readTarget(), target)) return { status: 'stale-target' };
    if (isBlocked()) return { status: 'edit-in-progress' };
    const contents = await readCopy();
    if (!editorTargetMatches(readTarget(), target)) return { status: 'stale-target' };
    if (isBlocked()) return { status: 'edit-in-progress' };
    if (contents === null) return { status: 'copy-not-found' };
    if (contents !== readDocument()) return { status: 'copy-does-not-match' };
    await loadCopy(contents);
    return { status: 'reopened' };
  };
}
