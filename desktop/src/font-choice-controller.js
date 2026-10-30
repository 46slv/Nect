import { samePrivateFontSource } from './private-font-source.js';
import { fontPickerCapability, sameFontFace } from './font-choice-ui.js';

/**
 * One picker, one fixed Session/revision/target. Discovery and preparation are
 * read-only; only Apply admits one Session command (or one static gesture).
 * Native/variable rows retain their existing prepare-on-Apply behavior.
 */
export function createFontChoiceController({
  owner,
  envelope,
  layerId,
  nodeId,
  source,
  characterIds,
  isCurrent,
  prepareWindows,
  preparePrivate,
  prepareVariable,
  onPreview = () => {},
  onStatus = () => {},
  onApplied = () => {},
}) {
  const original = structuredClone(source);
  const ids = characterIds === undefined ? undefined : [...characterIds];
  let serial = 0,
    closed = false,
    committing = false,
    activeKey;
  let ready = null,
    gesture = null,
    pending = Promise.resolve(),
    abort;
  const fresh = () =>
    !closed &&
    isCurrent() &&
    owner.state.sessionId === envelope.sessionId &&
    owner.state.expectedRevision === envelope.expectedRevision;
  const target = { layerId, nodeId, ...(ids === undefined ? {} : { characterIds: ids }) };
  function cancelGesture() {
    if (gesture && owner.hasGesture && owner.gestureState?.gestureId === gesture.gestureId)
      owner.dispatch({ ...owner.state, type: 'cancel-gesture', payload: gesture });
    gesture = null;
    onPreview(null);
  }
  function assertFresh() {
    if (!fresh()) throw Error('Document or selection changed; reopen Font');
  }
  function select(row) {
    const request = ++serial;
    abort?.abort();
    abort = new AbortController();
    ready = null;
    activeKey = row?.key;
    cancelGesture();
    const choice = row?.choice && structuredClone(row.choice);
    const capability = fontPickerCapability(original, choice, { characterIds: ids });
    if (!choice || !capability.route) {
      onStatus(capability.reason || 'Choose a font', false);
      pending = Promise.resolve();
      return pending;
    }
    pending = (async () => {
      try {
        assertFresh();
        const route = capability.route;
        if (route === 'set-variable-font' || route === 'set-os-font-text') {
          ready = { key: row.key, choice, route };
          onStatus('Ready · prepared on Apply', true);
          return;
        }
        onStatus('', false);
        if (route === 'replace-os-text-font') {
          ready = { key: row.key, choice, route };
          onStatus('Ready', true);
          return;
        }
        gesture = owner.dispatch({
          ...envelope,
          type: 'begin-gesture',
          payload: {
            target: { kind: 'text-font', ...target },
          },
        });
        gesture = owner.dispatch({
          ...envelope,
          type: 'update-gesture',
          payload: {
            ...gesture,
            update: { font: choice.id },
          },
        });
        onPreview(owner.previewSnapshot(gesture.gestureId).document);
        ready = { key: row.key, choice, route, gesture: true };
        onStatus('', true);
      } catch (error) {
        if (closed || request !== serial) return;
        cancelGesture();
        onStatus(error.message || String(error), false);
      }
    })();
    return pending;
  }
  async function apply(row) {
    if (!row || closed || committing || row.key !== activeKey) return false;
    committing = true;
    const request = serial;
    let variable;
    try {
      await pending;
      if (closed || serial !== request || ready?.key !== row.key) return false;
      assertFresh();
      const { choice, route } = ready;
      let result;
      if (ready.gesture) {
        result = owner.dispatch({ ...envelope, type: 'commit-gesture', payload: gesture });
        gesture = null;
      } else {
        let payload = { ...target };
        if (route === 'replace-os-text-font') payload.fontId = choice.id;
        else if (route === 'set-os-font-text') {
          onStatus('Preparing font; Escape cancels…', false);
          const settings = { ligatures: original.osFont?.outline?.ligatures ?? false };
          const prepared = choice.source === 'file-native'
            ? await preparePrivate(original, choice, { signal: abort.signal, settings })
            : await prepareWindows(original, choice.face, { signal: abort.signal, settings });
          if (choice.source === 'file-native' && !samePrivateFontSource(prepared?.source, choice.sourceRef))
            throw Error('Font response did not match the selected file; no change applied');
          const outline = choice.source === 'file-native' ? prepared?.outline : prepared;
          if (closed || request !== serial) return false;
          if (!sameFontFace(outline?.face, choice.face))
            throw Error('Font response did not match the selected face; no change applied');
          if (outline.ligatures !== settings.ligatures)
            throw Error('Font response did not preserve standard ligatures; no change applied');
          if (choice.source === 'file-native') payload.source = choice.sourceRef;
          else if (original.osFont?.source !== undefined) payload.source = null;
          payload.outline = outline;
          payload.settings = settings;
        } else if (route === 'set-variable-font') {
          onStatus('Preparing font; Escape cancels…', false);
          variable = await prepareVariable(
            {
              ...envelope,
              type: route,
              payload: {
                ...payload,
                fontId: choice.id,
                axes: choice.variable ? {} : null,
              },
            },
            { signal: abort.signal, choice },
          );
          if (
            !variable ||
            typeof variable.commit !== 'function' ||
            typeof variable.cancel !== 'function'
          )
            throw Error('Font preparation did not finish; no change applied');
          if (closed || request !== serial) {
            variable?.cancel();
            return false;
          }
        }
        if (closed || request !== serial) return false;
        assertFresh();
        result = variable
          ? variable.commit()
          : owner.dispatch({ ...envelope, type: route, payload });
      }
      closed = true;
      ready = null;
      onPreview(null);
      onApplied(result);
      return true;
    } catch (error) {
      if (typeof variable?.cancel === 'function') variable.cancel();
      if (!closed && request === serial) {
        ready = null;
        cancelGesture();
        onStatus(error.message || String(error), false);
      }
      return false;
    } finally {
      committing = false;
    }
  }
  return {
    select,
    apply,
    cancel() {
      if (closed) return;
      closed = true;
      ++serial;
      abort?.abort();
      ready = null;
      cancelGesture();
    },
    get closed() {
      return closed;
    },
  };
}
