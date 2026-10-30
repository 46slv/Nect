// Native sliders keep platform input behavior; the Session owns one complete edit.
export function createColorRangeEdit({
  app,
  selector,
  snapshot,
  restore,
  onCommit,
  onCancel,
  render,
}) {
  let gesture = null;
  const keys = [
    'ArrowUp',
    'ArrowDown',
    'ArrowLeft',
    'ArrowRight',
    'Home',
    'End',
    'PageUp',
    'PageDown',
  ];
  function stop(rollback = true) {
    const old = gesture;
    gesture = null;
    if (!old) return;
    if (rollback) {
      restore(old.before);
      old.input.value = old.value;
      onCancel();
    }
    try {
      if (old.input.hasPointerCapture?.(old.pointerId))
        old.input.releasePointerCapture(old.pointerId);
    } catch {}
  }
  app.addEventListener('pointerdown', (event) => {
    const input = event.target.closest?.(selector);
    if (!input || input.disabled || event.button !== 0 || event.isPrimary === false) return;
    if (gesture?.kind === 'pointer' && gesture.pointerId !== event.pointerId) return;
    stop();
    gesture = {
      kind: 'pointer',
      input,
      value: input.value,
      pointerId: event.pointerId,
      before: snapshot(),
    };
  });
  for (const type of ['pointerup', 'pointercancel', 'lostpointercapture'])
    app.addEventListener(type, (event) => {
      if (gesture?.kind !== 'pointer' || gesture.pointerId !== event.pointerId) return;
      event.stopImmediatePropagation();
      stop(type !== 'pointerup');
      if (type === 'pointerup') onCommit();
      render();
    });
  app.addEventListener('keydown', (event) => {
    if (event.isComposing || event.keyCode === 229) return;
    if (gesture && event.key === 'Escape') {
      event.preventDefault();
      event.stopImmediatePropagation();
      stop();
      render();
      return;
    }
    if (gesture?.kind === 'key' && event.key === 'Tab') {
      stop(false);
      onCommit();
      render();
      return;
    }
    const input = event.target.closest?.(selector);
    if (!input || input.disabled || !keys.includes(event.key) || gesture) return;
    gesture = { kind: 'key', input, value: input.value, before: snapshot() };
  });
  app.addEventListener('keyup', (event) => {
    if (gesture?.kind !== 'key' || !keys.includes(event.key)) return;
    stop(false);
    onCommit();
    render();
  });
  app.ownerDocument.defaultView?.addEventListener('blur', () => {
    if (!gesture) return;
    stop();
    render();
  });
  return {
    stop,
    restoreBaseline() {
      if (gesture) restore(gesture.before);
    },
    get active() {
      return !!gesture;
    },
  };
}
