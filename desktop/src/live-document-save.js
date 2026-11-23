import { captureEditorTarget, editorTargetMatches } from './live-editor-target.js';

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

// One boundary per admitted client lifetime. A receipt is not a durable file
// journal. When the in-memory ledger fills, refuse new writes rather than
// evicting an old ID that the caller might replay.
export function createLiveSaveAsBoundary({ readTarget, documents, receiptLimit = 64 }) {
  const performSave = createLiveDocumentSave({ readTarget, documents });
  const receipts = new Map();
  const validId = (id) => typeof id === 'string' && /^[A-Za-z0-9_-]{1,96}$/.test(id);
  return {
    inspect: () => ({
      saveEpoch: documents.saveEpoch,
      dirty: documents.dirty,
      pending: documents.saving,
    }),
    getReceipt(requestId) {
      if (!validId(requestId)) return { status: 'invalid-arguments' };
      return { ...(receipts.get(requestId)?.result ?? { status: 'unknown-request' }) };
    },
    async run({ target, saveEpoch, requestId }) {
      if (!validId(requestId) || !target || !Number.isSafeInteger(saveEpoch) || saveEpoch < 0)
        return { status: 'invalid-arguments' };
      const previous = receipts.get(requestId);
      if (previous) {
        return editorTargetMatches(previous.target, target) && previous.saveEpoch === saveEpoch
          ? { ...previous.result }
          : { status: 'request-conflict' };
      }
      if (documents.saveEpoch !== saveEpoch) return { status: 'stale-save-state' };
      if (!editorTargetMatches(readTarget(), target)) return { status: 'stale-target' };
      if (documents.saving || documents.transitioning) return { status: 'busy' };
      if (documents.hasPendingEdits()) return { status: 'edit-in-progress' };
      if (receipts.size >= receiptLimit) return { status: 'receipt-capacity' };

      const entry = {
        target: captureEditorTarget(target),
        saveEpoch,
        result: { status: 'pending' },
      };
      receipts.set(requestId, entry);
      try {
        const result = await performSave({ target: entry.target, saveAs: true });
        entry.result =
          result.status === 'accepted-by-writer' ? { ...result, saveEpoch: saveEpoch + 1 } : result;
      } catch {
        entry.result = { status: 'outcome-unknown' };
      }
      return { ...entry.result };
    },
  };
}
