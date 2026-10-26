import { validatePreviewDiscovery } from './update-discovery.js';

const freeze = (value) => {
  if (value && typeof value === 'object') {
    Object.values(value).forEach(freeze);
    Object.freeze(value);
  }
  return value;
};
const copy = (value) => freeze(structuredClone(value));
const key = (value) =>
  JSON.stringify(value, (_name, item) =>
    item && typeof item === 'object' && !Array.isArray(item)
      ? Object.fromEntries(
          Object.keys(item)
            .sort()
            .map((name) => [name, item[name]]),
        )
      : item,
  );
const unavailable = (reason) => ({ state: 'unavailable', reason });

// Adapters are supplied by offline tests only. Production supplies only provider=null.
// Context is metadata, never a publisher/package/native trust grant.
export function createManualUpdateCheck({
  provider = null,
  identity,
  nextRequest,
  readSource,
  clock,
  readContext = () => ({ provider, identity }),
  prepare = null,
} = {}) {
  const enabled = provider != null;
  let state = enabled
    ? { state: 'idle', reason: 'NOT_CHECKED' }
    : { state: 'unconfigured', reason: 'NO_PROVIDER' };
  let generation = 0,
    disposed = false,
    review = null;
  const listeners = new Set();
  const requests = new Set();
  const getState = () => structuredClone(state);
  const busy = () => ['checking', 'preparing'].includes(state.state);
  function publish(value) {
    state = copy(value);
    for (const listener of listeners) listener(getState());
    return getState();
  }
  function validateCurrent(bound) {
    try {
      if (key(copy(readContext())) !== key(bound.context)) return unavailable('REVIEW_STALE');
      return validatePreviewDiscovery({
        ...bound.context,
        request: bound.request,
        response: bound.response,
        get now() {
          return clock();
        },
      });
    } catch {
      return unavailable('REVIEW_STALE');
    }
  }
  function invalidate(reason = 'CANCELLED') {
    if (disposed || !enabled) return getState();
    generation++;
    review = null;
    return publish({ state: 'idle', reason });
  }
  async function check() {
    if (disposed || !enabled || busy()) return getState();
    const operation = ++generation;
    review = null;
    publish({ state: 'checking', reason: 'CHECKING' });
    if (disposed || operation !== generation) return getState();
    let bound, outcome;
    try {
      const context = copy(readContext());
      const request = copy(nextRequest());
      outcome = validatePreviewDiscovery({ ...context, request });
      if (outcome.reason === 'SOURCE_MISSING') {
        if (requests.has(request.requestId)) outcome = unavailable('REQUEST_REUSED');
        else {
          requests.add(request.requestId);
          let response;
          if (disposed || operation !== generation) return getState();
          try {
            response = copy(await readSource(request));
          } catch {
            response = { kind: 'transport-error' };
          }
          if (disposed || operation !== generation) return getState();
          bound = { context, request, response };
          outcome = validateCurrent(bound);
        }
      }
    } catch {
      outcome = unavailable('REQUEST_INVALID');
    }
    if (disposed || operation !== generation) return getState();
    if (outcome.state === 'update_advisory') {
      review = copy(bound);
      outcome = {
        ...outcome,
        reviewId: operation,
        confirmation: 'required',
        preparationAvailable: typeof prepare === 'function',
      };
    }
    return publish(outcome);
  }
  function refresh() {
    if (disposed || !review) return getState();
    const bound = review;
    const operation = generation;
    const outcome = validateCurrent(bound);
    if (disposed || operation !== generation || review !== bound) return getState();
    if (outcome.state !== 'update_advisory') {
      generation++;
      review = null;
      return publish(outcome);
    }
    return getState();
  }
  async function confirm(reviewId) {
    if (disposed || !review || state.state !== 'update_advisory') return getState();
    if (reviewId !== state.reviewId) {
      review = null;
      return publish(unavailable('REVIEW_STALE'));
    }
    const bound = review;
    const outcome = validateCurrent(bound);
    if (disposed || review !== bound) return getState();
    review = null; // Consent is single-use, including failures and missing adapters.
    if (outcome.state !== 'update_advisory') return publish(outcome);
    if (typeof prepare !== 'function') return publish(unavailable('PREPARATION_UNCONFIGURED'));
    const operation = ++generation;
    publish({ state: 'preparing', reason: 'PREPARING' });
    if (disposed || operation !== generation) return getState();
    let failure = false;
    try {
      await prepare(bound);
    } catch {
      failure = true;
    }
    if (disposed || operation !== generation) return getState();
    const current = validateCurrent(bound);
    if (disposed || operation !== generation) return getState();
    if (current.state !== 'update_advisory') return publish(current);
    return publish(
      failure
        ? unavailable('PREPARATION_FAILED')
        : { state: 'preparation_complete', reason: 'ADAPTER_COMPLETED' },
    );
  }
  return Object.freeze({
    check,
    confirm,
    refresh,
    getState,
    cancel: () => invalidate(),
    invalidate: () => invalidate('CONTEXT_CHANGED'),
    subscribe(listener) {
      listeners.add(listener);
      return () => listeners.delete(listener);
    },
    dispose() {
      if (disposed) return;
      generation++;
      review = null;
      disposed = true;
      publish({ state: 'disposed', reason: 'DISPOSED' });
      listeners.clear();
    },
  });
}
