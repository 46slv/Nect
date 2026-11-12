// This dialog owns only a draft. The caller owns selection, freshness and registration.
const idPattern = /^[a-zA-Z0-9][a-zA-Z0-9:_-]{0,180}$/;
const validLabel = (value) =>
  typeof value === 'string' && value.trim().length > 0 && value.length <= 100;

export function openMacroCaptureDialog({ document, candidates, anchor = document.activeElement }) {
  if (!Array.isArray(candidates) || !candidates.length || candidates.length > 4)
    throw Error('Expected one to four Macro controls');
  const ids = new Set(),
    targets = new Set();
  const controls = candidates.map((candidate) => {
    const { id, nodeId, key, label } = candidate ?? {};
    const target = `${nodeId}\u0000${key}`;
    if (
      typeof id !== 'string' ||
      !idPattern.test(id) ||
      typeof nodeId !== 'string' ||
      !idPattern.test(nodeId) ||
      !['scale', 'rotation'].includes(key) ||
      !validLabel(label) ||
      ids.has(id) ||
      targets.has(target)
    )
      throw Error('Invalid Macro control');
    ids.add(id);
    targets.add(target);
    return { id, nodeId, key, label };
  });

  const dialog = document.createElement('dialog');
  dialog.className = 'macro-capture-dialog';
  dialog.setAttribute('aria-label', 'Create Macro');
  dialog.innerHTML = `<form novalidate><h2>Create Macro</h2><label class="macro-capture-name">Name<input data-macro-name type="text" maxlength="100" autocomplete="off" required></label><fieldset><legend>Controls</legend><div data-macro-controls></div></fieldset><output role="status" aria-live="polite"></output><footer><button type="button" data-macro-cancel>Cancel</button><button type="submit">Create</button></footer></form>`;
  const name = dialog.querySelector('[data-macro-name]'),
    list = dialog.querySelector('[data-macro-controls]'),
    submit = dialog.querySelector('[type="submit"]'),
    cancel = dialog.querySelector('[data-macro-cancel]'),
    status = dialog.querySelector('output'),
    composing = new Set();
  const rows = controls.map((control) => {
    const row = document.createElement('div'),
      selected = document.createElement('input'),
      label = document.createElement('input');
    row.className = 'macro-capture-control';
    selected.type = 'checkbox';
    selected.dataset.macroControl = control.id;
    selected.setAttribute('aria-label', `Publish ${control.label}`);
    label.type = 'text';
    label.maxLength = 100;
    label.value = control.label;
    label.dataset.macroControlLabel = control.id;
    label.setAttribute('aria-label', `Name for ${control.label}`);
    label.disabled = true;
    row.append(selected, label);
    list.append(row);
    return { control, selected, label };
  });

  let settled = false,
    touched = false,
    resolveResult;
  const result = new Promise((resolve) => (resolveResult = resolve));
  function update() {
    const selected = rows.filter((row) => row.selected.checked);
    const nameValid = validLabel(name.value);
    const labelsValid = selected.every((row) => validLabel(row.label.value));
    name.setAttribute('aria-invalid', String(touched && !nameValid));
    for (const row of rows) {
      row.label.disabled = !row.selected.checked;
      row.label.setAttribute(
        'aria-invalid',
        String(row.selected.checked && !validLabel(row.label.value)),
      );
    }
    submit.disabled = !nameValid || !selected.length || !labelsValid || composing.size > 0;
    status.textContent =
      !touched || composing.size || (nameValid && labelsValid)
        ? ''
        : !nameValid
          ? 'Enter a name.'
          : 'Enter a name for each selected control.';
  }
  function finish(value) {
    if (settled) return;
    settled = true;
    if (dialog.open) dialog.close();
    dialog.remove();
    try {
      if (anchor?.isConnected && typeof anchor.focus === 'function')
        anchor.focus({ preventScroll: true });
    } catch {
      // A replaced focus target must not prevent settling the draft.
    }
    resolveResult(value);
  }
  const textInputs = [name, ...rows.map((row) => row.label)];
  for (const input of textInputs) {
    input.addEventListener('input', () => {
      touched = true;
      update();
    });
    input.addEventListener('compositionstart', () => {
      composing.add(input);
      update();
    });
    input.addEventListener('compositionend', () => {
      composing.delete(input);
      touched = true;
      update();
    });
  }
  for (const row of rows) row.selected.addEventListener('change', update);
  for (const type of ['pointerdown', 'click'])
    dialog.addEventListener(
      type,
      (event) => {
        if (!composing.size || composing.has(event.target)) return;
        event.preventDefault();
        event.stopImmediatePropagation();
      },
      true,
    );
  dialog.addEventListener('keydown', (event) => {
    event.stopPropagation();
    if (composing.size || event.isComposing || event.keyCode === 229) {
      if (event.key === 'Enter' || event.key === 'Escape') event.preventDefault();
      return;
    }
    if (event.key === 'Escape') {
      event.preventDefault();
      finish(null);
    }
  });
  dialog.addEventListener('cancel', (event) => {
    event.preventDefault();
    if (!composing.size) finish(null);
  });
  dialog.addEventListener('close', () => finish(null));
  cancel.addEventListener('click', () => {
    if (!composing.size) finish(null);
  });
  dialog.querySelector('form').addEventListener('submit', (event) => {
    event.preventDefault();
    update();
    if (submit.disabled || composing.size) return;
    finish({
      name: name.value,
      parameters: rows
        .filter((row) => row.selected.checked)
        .map(({ control, label }) => ({ ...control, label: label.value })),
    });
  });
  update();
  try {
    document.body.append(dialog);
    dialog.showModal();
    name.focus();
  } catch (error) {
    dialog.remove();
    throw error;
  }
  return { result, cancel: () => finish(null) };
}
