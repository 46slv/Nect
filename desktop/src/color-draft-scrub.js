import { scrubNumericValues } from './numeric-scrub.js';

// Color recipes are view drafts. One completed edit becomes one authored command/Undo.
export function createColorDraftScrub({
  app,
  selector,
  envelope,
  getState,
  snapshot,
  restore,
  update,
  onEdit,
  render,
  onCommit = () => {},
  onCancel = () => {},
}) {
  let gesture = null;
  const matchingInput = (input) =>
    [...app.querySelectorAll(selector)].find(
      (candidate) =>
        candidate.getAttribute('aria-label') === input.getAttribute('aria-label') &&
        candidate.dataset.wheelId === input.dataset.wheelId &&
        candidate.dataset.wheelSetting === input.dataset.wheelSetting &&
        candidate.dataset.sharedColorId === input.dataset.sharedColorId,
    );
  function showValue(d, value) {
    const input = matchingInput(d.input);
    if (input) input.value = String(value);
  }
  function stop(rollback = true) {
    const old = gesture;
    gesture = null;
    if (!old) return;
    if (rollback && old.started) {
      restore(old.before);
      showValue(old, old.original);
      onEdit();
      onCancel();
    }
    try {
      if (app.hasPointerCapture?.(old.pointerId)) app.releasePointerCapture(old.pointerId);
    } catch {}
  }
  app.addEventListener('pointerdown', (event) => {
    let input = event.target.closest?.(selector);
    if (
      !input ||
      input.disabled ||
      input.readOnly ||
      event.button !== 0 ||
      event.isPrimary === false
    )
      return;
    if (gesture && gesture.pointerId !== event.pointerId) return;
    const rect = input.getBoundingClientRect();
    if (rect.width && event.clientX >= rect.right - 18) return; // Native spinner.
    event.stopImmediatePropagation();
    try {
      stop();
      envelope(input);
      // Settle a typed value first, exactly as other numeric scrubs do.
      if (input.value !== input.defaultValue) {
        input.dispatchEvent(new app.ownerDocument.defaultView.Event('change', { bubbles: true }));
        input = matchingInput(input);
        if (!input || input.disabled || input.readOnly) return;
        envelope(input);
        input.focus?.({ preventScroll: true });
      }
      const original = Number(input.value);
      if (!input.value.trim() || !Number.isFinite(original)) return;
      gesture = {
        input,
        original,
        before: snapshot(input),
        state: { ...getState() },
        pointerId: event.pointerId,
        x: event.clientX,
        started: false,
        step: Number(input.dataset.colorScrubStep),
        min: Number(input.min),
        max: Number(input.max),
      };
      // #app survives draft renders, unlike the input itself.
      app.setPointerCapture?.(event.pointerId);
    } catch {
      stop();
    }
  });
  app.addEventListener('pointermove', (event) => {
    const d = gesture;
    if (!d || d.pointerId !== event.pointerId) return;
    event.stopImmediatePropagation();
    const state = getState();
    if (
      state.sessionId !== d.state.sessionId ||
      state.expectedRevision !== d.state.expectedRevision
    ) {
      stop();
      render();
      return;
    }
    if (!d.started && Math.abs(event.clientX - d.x) <= 4) return;
    event.preventDefault();
    d.started = true;
    try {
      const [value] = scrubNumericValues([d.original], event.clientX - d.x, d.step, d.min, d.max);
      restore(d.before); // Ring-count conversion always uses the fixed gesture baseline.
      update(d.input, value);
      showValue(d, value);
      onEdit();
      render();
    } catch {
      stop();
      render();
    }
  });
  for (const type of ['pointerup', 'pointercancel', 'lostpointercapture'])
    app.addEventListener(type, (event) => {
      if (!gesture || gesture.pointerId !== event.pointerId) return;
      event.stopImmediatePropagation();
      const started = gesture.started;
      if (started) event.preventDefault();
      stop(type !== 'pointerup');
      if (started && type === 'pointerup') onCommit();
      if (started) render();
    });
  app.addEventListener('keydown', (event) => {
    if (event.key !== 'Escape' || event.isComposing || event.keyCode === 229 || !gesture) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    stop();
    render();
  });
  app.ownerDocument.defaultView?.addEventListener('blur', () => {
    if (!gesture) return;
    stop();
    render();
  });
  return { stop };
}
