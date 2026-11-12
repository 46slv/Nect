import { MAX_COMPOSITION_DIMENSION } from './composition-project-model.js';
import { createNumericControlBehavior } from './numeric-control-behavior.js';

// The fields are a temporary proposal, not an edit to the open Project.
export function chooseNewProjectSize({ width, height, isCurrent }) {
  const previousFocus = document.activeElement;
  const dialog = document.createElement('dialog');
  dialog.className = 'new-project-dialog';
  dialog.setAttribute('aria-labelledby', 'new-project-heading');
  dialog.innerHTML = `<form novalidate><h3 id="new-project-heading">New project</h3><div class="new-project-dimensions"><label>Width · px<input name="width" type="number" min="0" max="${MAX_COMPOSITION_DIMENSION}" step="any" required title="Drag horizontally to adjust; click to type"></label><label>Height · px<input name="height" type="number" min="0" max="${MAX_COMPOSITION_DIMENSION}" step="any" required title="Drag horizontally to adjust; click to type"></label></div><output role="alert"></output><footer><button type="button" value="cancel" data-cancel>Cancel</button><button type="submit">Create project</button></footer></form>`;
  const inputs = [...dialog.querySelectorAll('input')],
    submit = dialog.querySelector('button[type="submit"]'),
    status = dialog.querySelector('output'),
    composing = new Set();
  inputs[0].value = String(width);
  inputs[1].value = String(height);
  let touched = false,
    scrub = null,
    settled = false;

  const validInput = (input) =>
    input.value !== '' &&
    Number.isFinite(input.valueAsNumber) &&
    input.valueAsNumber > 0 &&
    input.valueAsNumber <= MAX_COMPOSITION_DIMENSION &&
    !input.validity.badInput;
  const update = () => {
    const valid = inputs.every(validInput);
    for (const input of inputs) input.setAttribute('aria-invalid', String(!validInput(input)));
    submit.disabled = !valid || composing.size > 0 || !!scrub?.behavior.active;
    status.textContent =
      !valid && touched
        ? `Enter dimensions greater than 0 and at most ${MAX_COMPOSITION_DIMENSION} px.`
        : '';
  };
  const cancelScrub = () => {
    if (!scrub) return false;
    const active = scrub.behavior.active;
    scrub.behavior.cancel();
    scrub.input.classList.remove('scrubbing');
    scrub = null;
    update();
    return active;
  };
  const onMove = (event) => {
    if (!scrub || event.pointerId !== scrub.id) return;
    const current = scrub;
    try {
      if (current.behavior.move(event.clientX - current.x).cancelled) {
        cancelScrub();
        return;
      }
      if (current.behavior.active) {
        event.preventDefault();
        current.input.classList.add('scrubbing');
        update();
        try {
          current.input.setPointerCapture?.(event.pointerId);
        } catch {}
      }
    } catch {
      cancelScrub();
    }
  };
  const onEnd = (event) => {
    if (!scrub || event.pointerId !== scrub.id) return;
    const current = scrub;
    scrub = null;
    if (event.type === 'pointercancel') current.behavior.cancel();
    else current.behavior.finish();
    current.input.classList.remove('scrubbing');
    update();
  };

  for (const input of inputs) {
    input.addEventListener('input', () => {
      if (scrub) {
        scrub.behavior.finish();
        scrub.input.classList.remove('scrubbing');
        scrub = null;
      }
      touched = true;
      update();
    });
    input.addEventListener('compositionstart', () => {
      cancelScrub();
      composing.add(input);
      update();
    });
    input.addEventListener('compositionend', () => {
      composing.delete(input);
      touched = true;
      update();
    });
    input.addEventListener('pointerdown', (event) => {
      const rect = input.getBoundingClientRect();
      if (
        event.button !== 0 ||
        event.isPrimary === false ||
        composing.size ||
        !validInput(input) ||
        (rect.width && event.clientX >= rect.right - 18)
      )
        return;
      cancelScrub();
      let original;
      const behavior = createNumericControlBehavior({
        valid: () =>
          input.isConnected && !composing.size && (!!scrub?.behavior.active || validInput(input)),
        read: () => {
          original = input.value;
          return [input.valueAsNumber];
        },
        step: () => 1,
        bounds: () => [0, MAX_COMPOSITION_DIMENSION],
        begin() {},
        preview: ([value]) => {
          input.value = String(value);
          touched = true;
          update();
        },
        commit() {},
        cancel: () => {
          input.value = original;
          update();
        },
      });
      scrub = { input, id: event.pointerId, x: event.clientX, behavior };
    });
  }
  dialog.addEventListener('dragstart', (event) => {
    if (scrub?.behavior.active) event.preventDefault();
  });
  document.addEventListener('pointermove', onMove);
  document.addEventListener('pointerup', onEnd);
  document.addEventListener('pointercancel', onEnd);
  window.addEventListener('blur', cancelScrub);
  update();

  return new Promise((resolve, reject) => {
    let closeResult = null;
    const finish = (result, error = null) => {
      if (settled) return;
      settled = true;
      cancelScrub();
      document.removeEventListener('pointermove', onMove);
      document.removeEventListener('pointerup', onEnd);
      document.removeEventListener('pointercancel', onEnd);
      window.removeEventListener('blur', cancelScrub);
      dialog.remove();
      if (!result && isCurrent() && previousFocus?.isConnected)
        previousFocus.focus({ preventScroll: true });
      if (error) reject(error);
      else resolve(result);
    };
    const close = (result = null) => {
      closeResult = result;
      if (dialog.open) dialog.close();
      finish(result);
    };
    dialog.addEventListener('close', () => finish(closeResult));
    dialog.addEventListener('cancel', (event) => {
      event.preventDefault();
      if (!composing.size) close();
    });
    dialog.addEventListener('keydown', (event) => {
      if (event.key === 'Escape' && scrub?.behavior.active) {
        event.preventDefault();
        event.stopPropagation();
        cancelScrub();
      } else if (
        (composing.size || event.isComposing || event.keyCode === 229) &&
        ['Enter', 'Escape'].includes(event.key)
      ) {
        event.preventDefault();
        event.stopPropagation();
      }
    });
    dialog.querySelector('[data-cancel]').addEventListener('click', () => {
      if (!composing.size) close();
    });
    dialog.querySelector('form').addEventListener('submit', (event) => {
      event.preventDefault();
      if (
        composing.size ||
        scrub?.behavior.active ||
        event.isComposing ||
        !inputs.every(validInput)
      )
        return;
      if (!isCurrent()) {
        status.textContent = 'Project changed. Cancel and start again.';
        return;
      }
      close({ width: inputs[0].valueAsNumber, height: inputs[1].valueAsNumber });
    });
    try {
      document.body.append(dialog);
      dialog.showModal();
      inputs[0].focus();
      inputs[0].select();
    } catch (error) {
      finish(null, error);
    }
  });
}
