import { trimPathGeometry } from './path-edit-geometry.js';
import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';

export function evaluateTrimPath(_operation, read, context) {
  const source = read('path'),
    start = read('start'),
    end = read('end');
  const result = trimPathGeometry(source, { start, end, prefix: fingerprint(context.nodeId) });
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Trim Path; edit the source Text',
  );
  result.unstable = true;
  result.fingerprint = fingerprint(signature(result));
  return result;
}
