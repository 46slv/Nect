import { simplifyGeometry } from './simplify-geometry.js';
import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';

// Keep the same derived identities and character-routing boundary as the host.
export function evaluateSimplifyPath(_operation, read, context) {
  const source = read('path');
  const result = simplifyGeometry(source, {
    tolerance: read('tolerance'),
    prefix: fingerprint(context.nodeId),
  });
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Simplify Path; edit the source Text',
  );
  result.unstable = true;
  result.fingerprint = fingerprint(signature(result));
  return result;
}
