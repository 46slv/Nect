// Node-only endpoint. Never import this entry from renderer code.
import { parentPort } from 'node:worker_threads';
import { createVariableBackend } from '../src/variable-font/backend.js';

const backend = createVariableBackend();
parentPort.on('message', ({ id, revision, assetId, bytes, request }) => {
  try {
    parentPort.postMessage({ id, revision, result: backend.prepare(assetId, bytes, request) });
  } catch (error) {
    backend.clear();
    parentPort.postMessage({
      id,
      revision,
      error: typeof error.code === 'string' ? error.code : 'BACKEND_FAILURE',
    });
  }
});
