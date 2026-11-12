import { puckerBloatGeometry } from './pucker-bloat-geometry.js';
import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';

// The manual center remains lazy while automatic centering is active.
export function evaluatePuckerBloat(_operation, read, context) {
  const source = read('path'),
    autoCenter = read('autoCenter');
  const result = puckerBloatGeometry(source, {
    amount: read('amount'),
    autoCenter,
    ...(autoCenter ? {} : { center: read('center') }),
    prefix: fingerprint(context.nodeId),
  });
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Pucker & Bloat; edit the source Text',
  );
  result.unstable = true;
  result.fingerprint = fingerprint(signature(result));
  return result;
}
