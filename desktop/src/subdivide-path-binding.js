import { subdividePathGeometry } from './subdivide-geometry.js';
import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';

export function evaluateSubdividePath(_operation, read, context) {
  const source = read('path');
  const result = subdividePathGeometry(source, {
    segments: read('segments'),
    prefix: fingerprint(context.nodeId),
  });
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Subdivide Path; edit the source Text',
  );
  result.unstable = true;
  result.fingerprint = fingerprint(signature(result));
  return result;
}
