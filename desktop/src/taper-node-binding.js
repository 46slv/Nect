import { taperGeometry } from './taper-geometry.js';
import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';

export function evaluateTaper(operation, read, context) {
  const source = read('path');
  const result = taperGeometry(source, {
    amount: read('amount'),
    axis: operation,
    prefix: fingerprint(context.nodeId),
  });
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Taper; edit the source Text',
  );
  result.unstable = true;
  result.fingerprint = fingerprint(signature(result));
  return result;
}
