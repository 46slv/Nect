import { smoothGeometry } from './smooth-geometry.js';
import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';

export function evaluateSmoothPath(_operation, read, context) {
  const source = read('path');
  const result = smoothGeometry(source, {
    factor: read('factor'),
    iterations: read('iterations'),
    keepEndpoints: read('keepEndpoints'),
    prefix: fingerprint(context.nodeId),
  });
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Smooth Path; edit the source Text',
  );
  result.unstable = true;
  result.fingerprint = fingerprint(signature(result));
  return result;
}
