import { reversePathGeometry } from './reverse-path-geometry.js';
import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';

// Trusted handler, receives only a frozen stable node identity and the lazy reader.
export function evaluateReversePath(_operation, read, context) {
  const source = read('path');
  const result = reversePathGeometry(source, { prefix: fingerprint(context.nodeId) });
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Reverse Path; edit the source Text',
  );
  result.unstable = true;
  result.fingerprint = fingerprint(signature(result));
  return result;
}
