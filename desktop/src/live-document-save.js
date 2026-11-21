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

// An external caller needs more than the authored edit revision: save-as can
// rebind the same document without editing it. Return only a local save epoch
// and status, never its filesystem path or native binding token.
export function createLiveSaveAsBoundary({ readTarget, documents }) {
  const performSave = createLiveDocumentSave({ readTarget, documents });
  return {
    inspect: () => ({
      saveEpoch: documents.saveEpoch,
      dirty: documents.dirty,
      pending: documents.saving,
    }),
    async run({ target, saveEpoch }) {
      if (!Number.isSafeInteger(saveEpoch) || saveEpoch < 0) return { status: 'invalid-arguments' };
      if (documents.saveEpoch !== saveEpoch) return { status: 'stale-save-state' };
      try {
        const result = await performSave({ target, saveAs: true });
        return result.status === 'accepted-by-writer'
          ? { ...result, saveEpoch: saveEpoch + 1 }
          : result;
      } catch {
        return { status: 'outcome-unknown' };
      }
    },
  };
}
