// Stable editor-control identity across a structural render; no selector interpolation.
const focusTargets = 'input,select,textarea,button,[tabindex="0"],[data-workspace-focus-target]';
function identity(el) {
  const field = el.closest('.field[data-node]');
  const colorRow = el.closest('[data-shared-color-row]');
  const colorAction = colorRow && el.matches('[data-shared-color-copy-toggle]');
  return JSON.stringify({
    zone: el.closest('.inspector-body') ? 'inspector' : el.closest('.graph') ? 'graph' : 'app',
    node: el.closest('.node')?.dataset.id,
    field: field ? [field.dataset.node, field.dataset.key] : null,
    tag: el.tagName,
    id: el.id,
    color: colorAction ? colorRow.dataset.sharedColorRow : undefined,
    label: colorAction ? null : el.getAttribute('aria-label'),
    data: { ...el.dataset },
  });
}
export function captureFocus(root) {
  const el = root.ownerDocument.activeElement;
  if (!el || !root.contains(el) || !el.matches(focusTargets)) return null;
  return {
    identity: identity(el),
    colorRow: el.closest('[data-shared-color-row]')?.dataset.sharedColorRow,
    value: el.value,
    cleanNumber:
      el.matches('input[type=number]') && el.value === el.defaultValue && !el.validity.badInput,
    start: el.selectionStart,
    end: el.selectionEnd,
    direction: el.selectionDirection,
  };
}
export function restoreFocus(root, state) {
  if (!state) return;
  const el = [...root.querySelectorAll(focusTargets)].find(
    (candidate) =>
      identity(candidate) === state.identity &&
      !candidate.disabled &&
      !candidate.closest('[data-shared-color-row][hidden], [data-shared-color-row][inert]') &&
      (!candidate.hasAttribute('data-workspace-focus-target') ||
        !candidate.closest('[hidden], [inert]')),
  );
  if (!el) {
    const row =
      state.colorRow &&
      [...root.querySelectorAll('[data-shared-color-row]')].find(
        (candidate) => candidate.dataset.sharedColorRow === state.colorRow,
      );
    const filter = row?.hidden && root.querySelector('[data-shared-color-filter]');
    if (filter && !filter.closest('[hidden], [inert]')) filter.focus({ preventScroll: true });
    return;
  }
  el.focus({ preventScroll: true });
  // Keep a draft that has not emitted change yet. Committed edits live in the model.
  if (
    !state.cleanNumber &&
    el.matches('input:not([type=file]):not([type=checkbox]):not([type=color]),textarea')
  )
    el.value = state.value;
  if (state.start !== null && state.start !== undefined && el.setSelectionRange)
    try {
      el.setSelectionRange(state.start, state.end, state.direction);
    } catch {}
}
