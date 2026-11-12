import { mirrorGeometry } from './mirror-geometry.js';
import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';

export function evaluateMirror(operation, read, context) {
  const source = read('path');
  const result = mirrorGeometry(source, {
    center: read('center'),
    angle: operation === 'left-right' ? 90 : operation === 'up-down' ? 0 : read('angle'),
    keepOriginal: read('keepOriginal'),
    prefix: fingerprint(context.nodeId),
  });
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Mirror; edit the source Text',
  );
  result.unstable = true;
  result.fingerprint = fingerprint(signature(result));
  return result;
}
