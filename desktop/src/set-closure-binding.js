import { setClosureGeometry } from './set-closure-geometry.js';
import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';

export function evaluateSetClosure(_operation, read, context) {
  const source = read('path'),
    closed = read('closed');
  const result = setClosureGeometry(source, { closed, prefix: fingerprint(context.nodeId) });
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Set Closure; edit the upstream Text',
  );
  result.unstable = true;
  context.identity.setFingerprint(result, (currentEra) => [
    context.identity.provenance(currentEra),
    closed,
    context.identity.fingerprint(source, currentEra) || signature(source),
    signature(result),
  ]);
  return result;
}
