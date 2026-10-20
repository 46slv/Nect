// Layer list is front-to-back; authored layers and SVG paint are back-to-front.
export function installLayerReorder({ app, getState, commit, onError }) {
  const document = app.ownerDocument;
  let pending = null,
    suppressClick = false;
  const rowAt = (event) =>
    document
      .elementFromPoint(event.clientX, event.clientY)
      ?.closest?.('.layers .layer[data-layer]');
  const clear = () => {
    const old = pending;
    pending = null;
    if (old) {
      delete old.row.dataset.layerDragging;
      if (old.target) delete old.target.dataset.layerDrop;
      if (old.row.hasPointerCapture?.(old.pointerId)) old.row.releasePointerCapture(old.pointerId);
    }
    return old;
  };
  const valid = () => {
    const state = getState();
    return (
      pending &&
      pending.row.isConnected &&
      !state.busy &&
      state.document === pending.document &&
      state.envelope.sessionId === pending.envelope.sessionId &&
      state.envelope.expectedRevision === pending.envelope.expectedRevision
    );
  };
  const update = (event) => {
    if (!pending || pending.pointerId !== event.pointerId) return false;
    if (!valid()) {
      clear();
      return false;
    }
    if (!pending.moved && Math.hypot(event.clientX - pending.x, event.clientY - pending.y) < 6)
      return false;
    if (!pending.moved && event.type === 'pointermove')
      pending.row.setPointerCapture?.(event.pointerId);
    pending.moved = true;
    suppressClick = true;
    pending.row.dataset.layerDragging = '';
    if (pending.target) delete pending.target.dataset.layerDrop;
    pending.target = null;
    const target = rowAt(event);
    if (!target || !app.contains(target) || target.dataset.layer === pending.layerId) return false;
    const rect = target.getBoundingClientRect();
    const edge = event.clientY < rect.top + rect.height / 2 ? 'above' : 'below';
    const remaining = pending.document.layers.filter((layer) => layer.id !== pending.layerId);
    const index = remaining.findIndex((layer) => layer.id === target.dataset.layer);
    if (index < 0) return false;
    pending.beforeLayerId =
      edge === 'below' ? remaining[index].id : (remaining[index + 1]?.id ?? null);
    pending.target = target;
    target.dataset.layerDrop = edge;
    event.preventDefault();
    return true;
  };
  app.addEventListener('pointerdown', (event) => {
    if (event.button !== 0 || event.isPrimary === false) return;
    suppressClick = false;
    clear();
    const row = event.target.closest('.layers .layer[data-layer]');
    const control = event.target.closest('button,input,select,textarea,a,[contenteditable="true"]');
    if (
      !row ||
      (control && !control.matches('button[data-select-layer]')) ||
      event.shiftKey ||
      event.ctrlKey ||
      event.metaKey ||
      event.altKey
    )
      return;
    const state = getState();
    const layer = state.document.layers.find((layer) => layer.id === row.dataset.layer);
    if (state.busy || !layer || layer.locked) return;
    pending = {
      row,
      layerId: layer.id,
      pointerId: event.pointerId,
      x: event.clientX,
      y: event.clientY,
      document: state.document,
      envelope: { ...state.envelope },
    };
    row.focus({ preventScroll: true });
    event.preventDefault();
  });
  document.addEventListener('pointermove', update);
  document.addEventListener('pointerup', (event) => {
    if (!pending || pending.pointerId !== event.pointerId) return;
    const accepted = update(event);
    const old = clear();
    if (!accepted || !old?.target) return;
    try {
      commit({ layerId: old.layerId, beforeLayerId: old.beforeLayerId }, old.envelope);
    } catch (error) {
      onError(error);
    }
  });
  for (const type of ['pointercancel', 'lostpointercapture'])
    document.addEventListener(type, (event) => {
      if (pending?.pointerId === event.pointerId) clear();
    });
  document.defaultView.addEventListener('blur', clear);
  document.addEventListener(
    'keydown',
    (event) => {
      if (pending && event.key === 'Escape') {
        clear();
        event.preventDefault();
        event.stopImmediatePropagation();
      }
    },
    true,
  );
  // Pointer capture may deliver a compatibility click to the original name.
  // Consume only that drag's click; keyboard clicks and the next press stay normal.
  app.addEventListener(
    'click',
    (event) => {
      if (suppressClick && event.detail !== 0 && event.target.closest('.layers')) {
        suppressClick = false;
        event.preventDefault();
        event.stopImmediatePropagation();
      }
    },
    true,
  );
}
