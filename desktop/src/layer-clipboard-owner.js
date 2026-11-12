// Layer Copy retains a live source, but never transfers that source identity to
// a replacement project or to a different Layer that later reuses the same ID.
export function createLayerClipboardOwner() {
  const noRestore = Symbol('unavailable Layer history');
  let copied = null;
  return {
    get hasCopy() {
      return copied !== null;
    },
    copy({ owner, generation, compositionId, layerId }) {
      copied = { owner, generation, compositionId, layerId, restoreToken: null };
    },
    clear() {
      copied = null;
    },
    observe({ owner, generation, operation, history }) {
      if (!copied) return;
      if (copied.owner !== owner || copied.generation !== generation) {
        copied = null;
        return;
      }
      const exists = owner.hasLayer(copied.compositionId, copied.layerId);
      if (copied.restoreToken !== null) {
        if (exists && history.entryToken === copied.restoreToken) copied.restoreToken = null;
      } else if (!exists) {
        copied.restoreToken =
          (operation === 'undo' ? history.redoToken : history.undoToken) ?? noRestore;
      }
    },
    source({ owner, generation, compositionId }) {
      if (!copied) return null;
      if (copied.owner !== owner || copied.generation !== generation) {
        copied = null;
        return null;
      }
      if (
        copied.compositionId !== compositionId ||
        copied.restoreToken !== null ||
        !owner.hasLayer(copied.compositionId, copied.layerId)
      )
        return null;
      return copied.layerId;
    },
  };
}
