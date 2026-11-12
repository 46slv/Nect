import { polarTransformGeometry } from './polar-transform.js';
import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';

export function evaluatePolarTransform(_operation, read, context) {
  const source = read('path');
  const result = polarTransformGeometry(
    source,
    Object.fromEntries(
      ['origin', 'center', 'width', 'height', 'radius', 'angle', 'inverse'].map((key) => [
        key,
        read(key),
      ]),
    ),
  );
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Polar Transform; edit the source Text',
  );
  result.unstable = true;
  result.fingerprint = fingerprint(signature(result));
  return result;
}
