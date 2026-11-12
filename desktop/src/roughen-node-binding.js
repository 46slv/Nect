import { roughenGeometry } from './roughen-geometry.js';
import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';

export function evaluateRoughen(_operation, read, context) {
  const source = read('path');
  const result = roughenGeometry(source, {
    amount: read('amount'),
    seed: read('seed'),
    keepEndpoints: read('keepEndpoints'),
    prefix: fingerprint(context.nodeId),
  });
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Roughen; edit the source Text',
  );
  result.unstable = true;
  result.fingerprint = fingerprint(signature(result));
  return result;
}
