// Node-only transport for the same bounded preparation protocol as the browser worker.
import { Worker } from 'node:worker_threads';

export function createNodeVariableWorkerTransport() {
  const workers = new Set();
  const terminations = new Set();
  let closed = false;
  return {
    workerFactory() {
      if (closed) throw Error('DISPOSED');
      const worker = new Worker(new URL('./variable-font-node-worker.js', import.meta.url));
      let stopped = false;
      const bridge = {
        postMessage: (data) => worker.postMessage(data),
        terminate() {
          if (stopped) return;
          stopped = true;
          workers.delete(bridge);
          const termination = worker.terminate();
          terminations.add(termination);
          termination.then(
            () => terminations.delete(termination),
            () => terminations.delete(termination),
          );
          return termination;
        },
      };
      worker.on('message', (data) => bridge.onmessage?.({ data }));
      worker.on('messageerror', (error) => bridge.onmessageerror?.(error));
      worker.on('error', (error) => bridge.onerror?.(error));
      // Even a clean unexpected exit must reject pending work instead of waiting for timeout.
      worker.on('exit', () => {
        workers.delete(bridge);
        if (!stopped) bridge.onerror?.(new Error('WORKER_EXIT'));
      });
      workers.add(bridge);
      return bridge;
    },
    async dispose() {
      closed = true;
      for (const worker of workers) worker.terminate();
      await Promise.all([...terminations]);
    },
  };
}
