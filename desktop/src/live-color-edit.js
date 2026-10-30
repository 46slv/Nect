// One bounded Session gesture owns an edit; only the latest requested frame is evaluated.
export function createLiveColorEdit({
  app,
  getSession,
  getDocument,
  proposal,
  onPreview,
  onSettled,
  notify,
}) {
  let edit = null,
    pending = false,
    frame = null,
    projected = null;
  const view = app.ownerDocument.defaultView;
  const samples = [];
  let requested = 0,
    evaluated = 0;
  const now = () => view.performance?.now?.() ?? Date.now();
  function cancelFrame() {
    if (frame !== null) view.cancelAnimationFrame?.(frame);
    frame = null;
  }
  function cancel() {
    cancelFrame();
    const old = edit;
    edit = null;
    pending = false;
    projected = null;
    if (old?.token)
      try {
        old.owner.dispatch({ ...old.owner.state, type: 'cancel-gesture', payload: old.token });
      } catch {}
    onSettled();
  }
  function flush() {
    cancelFrame();
    if (!pending || !edit) return;
    pending = false;
    const start = now(),
      owner = getSession(),
      state = owner.state;
    try {
      if (
        owner !== edit.owner ||
        state.sessionId !== edit.state.sessionId ||
        state.expectedRevision !== edit.state.expectedRevision
      )
        throw Error('Color edit is stale; use the current document');
      if (!edit.token)
        edit.token = owner.dispatch({
          ...state,
          type: 'begin-gesture',
          payload: {
            target: {
              kind: 'color-slots',
              slotIds: getDocument().colorSlots.map((slot) => slot.id),
            },
          },
        });
      const update = proposal();
      edit.token = owner.dispatch({
        ...state,
        type: 'update-gesture',
        payload: { ...edit.token, update },
      });
      projected = owner.previewSnapshot(edit.token.gestureId).document;
      onPreview();
      evaluated++;
      const document = getDocument();
      samples.push({
        durationMs: now() - start,
        slots: update.updates.length,
        layers: document.layers?.length || 0,
        nodes: document.layers?.reduce((count, layer) => count + layer.nodes.length, 0) || 0,
      });
      if (samples.length > 120) samples.shift();
    } catch (error) {
      cancel();
      notify(error.message, true);
      onPreview();
    }
  }
  function schedule() {
    requested++;
    if (!edit) edit = { owner: getSession(), state: { ...getSession().state }, token: null };
    pending = true;
    if (frame === null) frame = view.requestAnimationFrame(flush);
  }
  function commit() {
    flush(); // The last input wins even if its animation frame has not run yet.
    if (!edit) return;
    const old = edit;
    edit = null;
    projected = null;
    try {
      if (old.token)
        old.owner.dispatch({ ...old.owner.state, type: 'commit-gesture', payload: old.token });
    } catch (error) {
      try {
        old.owner.dispatch({ ...old.owner.state, type: 'cancel-gesture', payload: old.token });
      } catch {}
      notify(error.message, true);
    }
    onSettled();
  }
  return {
    schedule,
    flush,
    commit,
    cancel,
    get active() {
      return !!edit;
    },
    get document() {
      return projected;
    },
    metrics: () => ({ requested, evaluated, samples: samples.map((sample) => ({ ...sample })) }),
  };
}
