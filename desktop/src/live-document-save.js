import { editorTargetMatches } from './live-editor-target.js';

// Admission and snapshot capture stay on the editor turn. The lifecycle, not
// the caller, owns the destination, the native picker and saved revision.
export function createLiveDocumentSave({ readTarget, documents }) {
  return async function save({ target, saveAs }) {
    if (!editorTargetMatches(readTarget(), target)) return { status: 'stale-target' };
    if (typeof saveAs !== 'boolean') return { status: 'invalid-arguments' };
    if (documents.saving || documents.transitioning) return { status: 'busy' };
    if (documents.hasPendingEdits()) return { status: 'edit-in-progress' };
    return { status: (await documents.save(saveAs)) ? 'accepted-by-writer' : 'not-confirmed' };
  };
}
