import { offsetClosedPath, OFFSET_EDITOR_LIMITS, OffsetError } from './closed-offset.js';
import { offsetGroupedClosedPathDetailed } from './grouped-closed-offset.js';
import { closedOffsetSettings } from './closed-offset-node-settings.js';
import { routeCharacters } from './character-geometry.js';
import { signature } from './path-signature.js';

export function evaluateClosedOffset(operation, read, context) {
  const source = read('path');
  if (!source) throw Error('Connect a Path input');
  const { amount, miterLimit } = closedOffsetSettings(read);
  if (!['miter', 'bevel', 'round'].includes(operation))
    throw new OffsetError('OFFSET_OPTIONS', 'Invalid Offset join');
  if (amount === 0) return source;
  const options = { join: operation, miterLimit, prefix: context.nodeId };
  const result = source.contours.some((contour) => contour.group !== undefined)
    ? offsetGroupedClosedPathDetailed(source, amount, options, OFFSET_EDITOR_LIMITS).path
    : offsetClosedPath(source, amount, options, OFFSET_EDITOR_LIMITS);
  result.unstable = true;
  context.identity.setFingerprint(result, (currentEra) => [
    context.identity.provenance(currentEra),
    amount,
    operation,
    miterLimit,
    context.identity.fingerprint(source, currentEra),
    source.contours,
    signature(result),
  ]);
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Closed Path Offset',
  );
  return result;
}
