import { LIMITS, runKey, freeze, requireThat as check } from './contract.js';
let activeWorkers = 0;
const error = (code) => Object.assign(new Error(code), { code });
/** One private byte store, one worker, one in-flight candidate. Never falls back to UI parsing. */
export function createVariablePreparer({
  workerFactory = () => new Worker(new URL('./worker.js', import.meta.url), { type: 'module' }),
  timeout = LIMITS.timeout,
} = {}) {
  check(Number.isFinite(timeout) && timeout > 0 && timeout <= LIMITS.timeout, 'INVALID_TIMEOUT');
  let worker,
    pending,
    sequence = 0,
    disposed = false,
    totalBytes = 0,
    revision = 0,
    cachedCommands = 0,
    reservedBytes = 0,
    registrations = 0;
  const assets = new Map(),
    ready = new Map();
  const stop = () => {
    if (worker) {
      worker.terminate();
      worker = undefined;
      activeWorkers--;
    }
  };
  const finish = (failure, result) => {
    if (!pending) return;
    const p = pending;
    pending = undefined;
    clearTimeout(p.timer);
    p.signal?.removeEventListener('abort', p.abort);
    if (failure) p.reject(error(failure));
    else p.resolve(result);
  };
  const abort = (code) => {
    stop();
    finish(code);
  };
  function ensureWorker() {
    if (worker) return;
    check(activeWorkers === 0, 'WORKER_LIMIT');
    worker = workerFactory();
    activeWorkers++;
    const currentWorker = worker;
    worker.onmessage = ({ data }) => {
      if (
        worker !== currentWorker ||
        !pending ||
        data.id !== pending.id ||
        data.revision !== pending.revision
      )
        return;
      if (data.revision !== revision) {
        abort('STALE_RESULT');
        return;
      }
      if (data.error) {
        abort(data.error);
        return;
      }
      const result = data.result;
      if (
        !result ||
        result.assetId !== pending.assetId ||
        !Array.isArray(result.commands) ||
        result.commands.length > LIMITS.commands ||
        new TextEncoder().encode(JSON.stringify(result)).length > LIMITS.outputBytes
      ) {
        abort('INVALID_WORKER_RESULT');
        return;
      }
      freeze(result);
      while (
        ready.size &&
        (ready.size >= LIMITS.runs ||
          cachedCommands + result.commands.length > LIMITS.cachedCommands)
      ) {
        const key = ready.keys().next().value;
        cachedCommands -= ready.get(key).commands.length;
        ready.delete(key);
      }
      ready.set(pending.key, result);
      cachedCommands += result.commands.length;
      finish(null, result);
    };
    worker.onerror = () => {
      if (worker === currentWorker) abort('WORKER_FAILURE');
    };
    worker.onmessageerror = () => {
      if (worker === currentWorker) abort('WORKER_FAILURE');
    };
  }
  return {
    async register(bytes) {
      check(!disposed, 'DISPOSED');
      check(
        bytes instanceof Uint8Array && bytes.byteLength >= 12 && bytes.byteLength <= LIMITS.bytes,
        'FONT_BYTES',
      );
      // Own the only parent copy. Never expose or transfer its backing buffer.
      check(
        assets.size + registrations < LIMITS.assets &&
          totalBytes + reservedBytes + bytes.byteLength <= LIMITS.totalBytes,
        'ASSET_LIMIT',
      );
      const owned = new Uint8Array(bytes);
      registrations++;
      reservedBytes += owned.byteLength;
      let id;
      try {
        id = Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256', owned)), (x) =>
          x.toString(16).padStart(2, '0'),
        ).join('');
      } finally {
        registrations--;
        reservedBytes -= owned.byteLength;
      }
      check(!disposed, 'DISPOSED');
      if (!assets.has(id)) {
        check(
          assets.size < LIMITS.assets && totalBytes + owned.byteLength <= LIMITS.totalBytes,
          'ASSET_LIMIT',
        );
        assets.set(id, owned);
        totalBytes += owned.byteLength;
      }
      return id;
    },
    setRevision(value) {
      check(Number.isSafeInteger(value) && value >= 0, 'INVALID_REVISION');
      revision = value;
      if (pending && pending.revision !== value) abort('STALE_RESULT');
    },
    prepare(assetId, request, { signal } = {}) {
      try {
        check(!disposed, 'DISPOSED');
        check(!pending, 'BUSY');
        check(assets.has(assetId), 'UNKNOWN_ASSET');
        const key = runKey(assetId, request);
        if (signal?.aborted) return Promise.reject(error('CANCELLED'));
        if (ready.has(key)) return Promise.resolve(ready.get(key));
        ensureWorker();
        return new Promise((resolve, reject) => {
          const id = ++sequence;
          pending = {
            id,
            revision,
            key,
            assetId,
            resolve,
            reject,
            signal,
            abort: () => abort('CANCELLED'),
          };
          pending.timer = setTimeout(() => abort('TIMEOUT'), timeout);
          signal?.addEventListener('abort', pending.abort, { once: true });
          try {
            worker.postMessage({ id, revision, assetId, bytes: assets.get(assetId), request });
          } catch {
            abort('WORKER_FAILURE');
          }
        });
      } catch (e) {
        return Promise.reject(e);
      }
    },
    read(assetId, request) {
      check(!disposed, 'DISPOSED');
      const value = ready.get(runKey(assetId, request));
      check(value, 'PREPARATION_REQUIRED');
      return value;
    },
    releaseWorker() {
      check(!pending, 'BUSY');
      stop();
    },
    dispose() {
      disposed = true;
      abort('DISPOSED');
      assets.clear();
      ready.clear();
      totalBytes = cachedCommands = 0;
    },
    stats() {
      return {
        assets: assets.size,
        bytes: totalBytes,
        runs: ready.size,
        cachedCommands,
        worker: Boolean(worker),
        pending: Boolean(pending),
      };
    },
  };
}
