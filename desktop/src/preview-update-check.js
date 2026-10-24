import { validatePreviewDiscovery } from './update-discovery.js';
import { renderDiscovery } from './build-info.js';

// Fixture client only: the production entry graph never imports this module.
// No source, native identity, clock or request is obtained implicitly.
export function createPreviewFixtureCheck({
  element,
  document,
  identity,
  provider,
  nextRequest,
  readFixture,
  clock,
}) {
  const button = document.createElement('button');
  button.type = 'button';
  const information = document.createElement('div');
  information.setAttribute('role', 'status');
  information.setAttribute('aria-live', 'polite');
  element.replaceChildren(button, information);
  let state =
    provider == null
      ? { state: 'unconfigured', reason: 'NO_PROVIDER' }
      : { state: 'idle', reason: 'NOT_CHECKED' };
  let pending = false;
  let disposed = false;

  function render() {
    renderDiscovery(information, state, document);
    if (state.state === 'idle') information.firstChild.textContent = 'Preview check not run';
    if (pending) information.firstChild.textContent = 'Checking preview metadata';
    if (state.reason === 'SOURCE_EXPIRED')
      information.firstChild.textContent = 'Preview metadata expired';
    button.textContent =
      state.state === 'unavailable' ? 'Retry preview check' : 'Check preview metadata';
    button.disabled = disposed || pending || state.state === 'unconfigured';
    element.setAttribute('aria-busy', String(pending));
  }

  function getState() {
    return { ...state, ...(state.advisory ? { advisory: { ...state.advisory } } : {}) };
  }

  async function check() {
    if (disposed || pending || state.state === 'unconfigured') return getState();
    pending = true;
    state = { state: 'checking', reason: 'CHECKING' };
    render();
    let outcome;
    try {
      let request;
      try {
        request = Object.freeze({ ...nextRequest() });
      } catch {
        outcome = { state: 'unavailable', reason: 'REQUEST_INVALID' };
      }
      if (!outcome) {
        const admission = validatePreviewDiscovery({ identity, provider, request });
        if (admission.reason !== 'SOURCE_MISSING') outcome = admission;
      }
      if (!outcome) {
        let response;
        try {
          response = await readFixture(request);
        } catch {
          response = { kind: 'transport-error' };
        }
        if (disposed) return getState();
        // Read the injected clock at completion, so a pending check can expire.
        outcome = validatePreviewDiscovery({
          identity,
          provider,
          request,
          response,
          get now() {
            return clock();
          },
        });
      }
      if (!disposed) state = outcome;
    } finally {
      pending = false;
      if (!disposed) render();
    }
    return getState();
  }

  const onClick = () => {
    void check();
  };
  button.addEventListener('click', onClick);
  render();
  return Object.freeze({
    check,
    getState,
    dispose() {
      disposed = true;
      button.removeEventListener('click', onClick);
      button.disabled = true;
      element.setAttribute('aria-busy', 'false');
    },
  });
}
