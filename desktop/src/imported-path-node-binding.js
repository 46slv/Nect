import { signature } from './path-signature.js';

export function evaluateImportedPath(_operation, _read, context) {
  const result = context.parsePathData();
  result.unstable = true;
  result.fingerprint = signature(result);
  return result;
}
