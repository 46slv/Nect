import { setPathStartGeometry } from './path-edit-geometry.js';
import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';

export function evaluateSetPathStart(_operation, read, context) {
  const source = read('path'),
    start = read('start');
  const result = setPathStartGeometry(source, { start, prefix: fingerprint(context.nodeId) });
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Set Path Start; edit the source Text',
  );
  result.unstable = true;
  result.fingerprint = fingerprint(signature(result));
  return result;
}
