import { expandStrokeGeometry } from './expand-stroke-geometry.js';
import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';

// Keep the existing derived identities and character-routing boundary.
export function evaluateExpandStroke(_operation, read, context) {
  const source = read('path');
  const result = expandStrokeGeometry(source, {
    prefix: fingerprint(context.nodeId),
    startWidthMultiplier: read('startWidthMultiplier'),
    endWidthMultiplier: read('endWidthMultiplier'),
  });
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Expand Stroke; edit the source Text',
  );
  result.unstable = true;
  result.fingerprint = fingerprint(signature(result));
  return result;
}
