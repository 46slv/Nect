// One consumer for Build and offline fixture clients. No adapters or trust are created here.
export function mountManualUpdateCheck({ element, document, controller, renderDiscovery }) {
  const information = document.createElement('div');
  information.setAttribute('role', 'status');
  information.setAttribute('aria-live', 'polite');
  const check = document.createElement('button');
  const cancel = document.createElement('button');
  const confirm = document.createElement('button');
  for (const button of [check, cancel, confirm]) button.type = 'button';
  cancel.textContent = 'Cancel';
  confirm.textContent = 'Confirm preparation';
  const onCheck = () => {
    void controller.check();
  };
  const onCancel = () => controller.cancel();
  const onConfirm = () => {
    void controller.confirm(controller.getState().reviewId);
  };
  check.addEventListener('click', onCheck);
  cancel.addEventListener('click', onCancel);
  confirm.addEventListener('click', onConfirm);
  function render(state) {
    renderDiscovery(information, state, document);
    const labels = {
      idle: 'Preview check not run',
      checking: 'Checking preview metadata',
      preparing: 'Preparing',
      preparation_complete: 'Preparation adapter completed',
      disposed: 'Check closed',
    };
    if (labels[state.state]) information.firstChild.textContent = labels[state.state];
    if (state.reason === 'SOURCE_EXPIRED')
      information.firstChild.textContent = 'Preview metadata expired';
    if (state.reason === 'REVIEW_STALE') information.firstChild.textContent = 'Check again';
    const pending = ['checking', 'preparing'].includes(state.state);
    check.textContent =
      state.state === 'unavailable' ? 'Retry preview check' : 'Check preview metadata';
    check.disabled = pending || state.state === 'disposed';
    cancel.hidden = !pending && state.state !== 'update_advisory';
    confirm.hidden = state.state !== 'update_advisory' || !state.preparationAvailable;
    confirm.disabled = state.state !== 'update_advisory';
    element.setAttribute('aria-busy', String(pending));
    if (state.state === 'unconfigured') element.replaceChildren(information);
    else if (check.parentNode !== element)
      element.replaceChildren(check, information, cancel, confirm);
  }
  const unsubscribe = controller.subscribe(render);
  render(controller.getState());
  return Object.freeze({
    ...controller,
    dispose() {
      controller.dispose();
      unsubscribe();
      check.removeEventListener('click', onCheck);
      cancel.removeEventListener('click', onCancel);
      confirm.removeEventListener('click', onConfirm);
    },
  });
}
