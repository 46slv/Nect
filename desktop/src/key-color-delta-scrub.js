import { createNumericControlBehavior } from './numeric-control-behavior.js';

// Detached recipe inputs keep their existing Preview/Apply and Session owner.
export function createKeyColorDeltaScrub(dialog, { valid, blocked, onActivityChange = () => {} }) {
  const document = dialog.ownerDocument;
  let scrub = null,
    internalInput = false;

  function finish(commit = false) {
    if (!scrub) return false;
    const old = scrub;
    scrub = null;
    document.removeEventListener('pointermove', move, true);
    for (const type of ['pointerup', 'pointercancel', 'lostpointercapture'])
      document.removeEventListener(type, end, true);
    document.defaultView?.removeEventListener('blur', blur);
    if (commit) old.behavior.finish();
    else old.behavior.cancel();
    onActivityChange();
    return true;
  }
  function blur() {
    finish();
  }
  function move(event) {
    if (!scrub || event.pointerId !== scrub.id) return;
    const active = scrub,
      result = active.behavior.move(event.clientX - active.x);
    if (result.cancelled) finish();
    else if (active.behavior.active) {
      event.preventDefault();
      try {
        active.input.setPointerCapture(event.pointerId);
      } catch {}
    }
  }
  function end(event) {
    if (!scrub || event.pointerId !== scrub.id) return;
    finish(event.type === 'pointerup');
  }
  function emit(input, raw) {
    input.value = raw;
    internalInput = true;
    try {
      input.dispatchEvent(new document.defaultView.Event('input', { bubbles: true }));
    } finally {
      internalInput = false;
    }
  }
  dialog.addEventListener(
    'pointerdown',
    (event) => {
      if (!scrub) return;
      event.preventDefault();
      event.stopImmediatePropagation();
    },
    true,
  );
  dialog.addEventListener('pointerdown', (event) => {
    const input = event.target.closest?.(
      '[data-key-color-input="deltaL"],[data-key-color-input="deltaRatioCmax"]',
    );
    if (
      !input ||
      !dialog.contains(input) ||
      scrub ||
      event.button !== 0 ||
      event.isPrimary === false
    )
      return;
    if (blocked()) {
      event.preventDefault();
      return;
    }
    const rect = input.getBoundingClientRect(),
      raw = input.value,
      number = Number(raw);
    if (
      (rect.width && event.clientX >= rect.right - 18) ||
      input.disabled ||
      input.readOnly ||
      !raw.trim() ||
      !Number.isFinite(number) ||
      !valid()
    )
      return;
    let original,
      changed = false;
    const behavior = createNumericControlBehavior({
      valid: () =>
        input.isConnected &&
        dialog.contains(input) &&
        !input.disabled &&
        !input.readOnly &&
        !blocked() &&
        valid(),
      read: () => {
        original = input.value;
        return [Number(original)];
      },
      step: () => 0.01,
      bounds: () => [-Number.MAX_VALUE, Number.MAX_VALUE],
      begin() {},
      preview([next]) {
        if (next === Number(original)) {
          if (changed) emit(input, original);
          changed = false;
          return;
        }
        changed = true;
        emit(input, String(next));
      },
      commit() {},
      cancel() {
        if (changed && input.isConnected) emit(input, original);
      },
    });
    scrub = { behavior, input, id: event.pointerId, x: event.clientX };
    onActivityChange();
    document.addEventListener('pointermove', move, true);
    for (const type of ['pointerup', 'pointercancel', 'lostpointercapture'])
      document.addEventListener(type, end, true);
    document.defaultView?.addEventListener('blur', blur);
  });
  dialog.addEventListener('input', (event) => {
    if (scrub && !internalInput) {
      const raw = event.target.value;
      finish();
      event.target.value = raw;
    }
  });
  dialog.addEventListener('compositionstart', () => finish());
  dialog.addEventListener('focusout', (event) => {
    if (scrub?.input === event.target) finish();
  });
  dialog.addEventListener('dragstart', (event) => {
    if (scrub?.input === event.target) event.preventDefault();
  });
  return {
    get active() {
      return !!scrub;
    },
    cancel: () => finish(),
    dispose: () => finish(),
  };
}
