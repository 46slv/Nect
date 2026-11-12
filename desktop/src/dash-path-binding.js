import { dashPathGeometry } from './path-edit-geometry.js';
import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';

export function evaluateDashPath(_operation, read, context) {
  const source = read('path');
  const result = dashPathGeometry(source, {
    dashLength: read('dashLength'),
    gapLength: read('gapLength'),
    offset: read('offset'),
    prefix: fingerprint(context.nodeId),
  });
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Dash Path; edit the source Text',
  );
  result.unstable = true;
  result.fingerprint = fingerprint(signature(result));
  return result;
}
