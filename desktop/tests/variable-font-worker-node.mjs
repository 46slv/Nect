// Test transport only. Production uses the browser Worker entry.
import { parentPort } from 'node:worker_threads';
import { createVariableBackend } from '../src/variable-font/backend.js';
const backend = createVariableBackend();
parentPort.on('message', ({ id, revision, assetId, bytes, request }) => {
  try {
    parentPort.postMessage({ id, revision, result: backend.prepare(assetId, bytes, request) });
  } catch (e) {
    backend.clear();
    parentPort.postMessage({ id, revision, error: e.code || 'BACKEND_FAILURE' });
  }
});
