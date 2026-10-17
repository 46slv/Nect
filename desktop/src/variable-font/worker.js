import { createVariableBackend } from './backend.js';
const backend = createVariableBackend();
self.onmessage = ({ data }) => {
  const { id, revision, assetId, bytes, request } = data;
  try {
    self.postMessage({ id, revision, result: backend.prepare(assetId, bytes, request) });
  } catch (error) {
    backend.clear();
    self.postMessage({
      id,
      revision,
      error: typeof error.code === 'string' ? error.code : 'BACKEND_FAILURE',
    });
  }
};
