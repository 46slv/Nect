import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';
import { instancePointsGeometry } from './instance-points-geometry.js';

export function evaluateInstancePoints(_operation, read, context) {
  if (!context.hasConnectedInput('path') || !context.hasConnectedInput('points'))
    throw Error('Connect both Source and Points Path inputs');
  const source = read('path'),
    targets = read('points'),
    alignTangent = read('alignTangent');
  if (!source || !targets) throw Error('Connect both Source and Points Path inputs');

  // Keep malformed or over-budget geometry away from Paper bounds work.
  // The geometry helper remains the authoritative structural validator.
  const cheapPointCount = (path) => {
    if (!Array.isArray(path?.contours)) return null;
    if (path.contours.length > 10000) throw Error('Instance on Points exceeds geometry budget');
    let count = 0;
    for (const contour of path.contours) {
      count += Array.isArray(contour?.points) ? contour.points.length : 0;
      if (count > 100000) throw Error('Instance on Points exceeds geometry budget');
    }
    return count;
  };
  const sourcePoints = cheapPointCount(source),
    targetPoints = cheapPointCount(targets);
  let result;
  if (sourcePoints === null || targetPoints === null) {
    result = instancePointsGeometry(source, targets, {
      prefix: fingerprint(context.nodeId),
      alignTangent,
    });
  } else if (sourcePoints === 0 || targetPoints === 0) {
    // Empty inputs produce empty geometry without constructing Paper bounds.
    result = instancePointsGeometry(source, targets, {
      prefix: fingerprint(context.nodeId),
      alignTangent,
    });
  } else {
    if (targetPoints > 256) throw Error('Instance on Points exceeds instance budget');
    const sourceGroups = new Set(source.contours.map((contour) => contour?.group ?? ''));
    if (
      source.contours.length * targetPoints > 10000 ||
      sourcePoints * targetPoints > 100000 ||
      sourceGroups.size * targetPoints > 256
    )
      throw Error('Instance on Points exceeds geometry budget');
    const bounds = context.pathBounds(source),
      pivot = [bounds[0][1], bounds[1][1]],
      prefix = fingerprint(context.nodeId);
    result = instancePointsGeometry(source, targets, { pivot, prefix, alignTangent });
  }
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Instance on Points; edit the source Text',
  );
  result.unstable = true;
  context.identity.setFingerprint(result, (currentEra) => [
    context.identity.provenance(currentEra),
    context.identity.fingerprint(source, currentEra) || null,
    signature(source),
    alignTangent,
    targets.contours.flatMap((contour) => contour.points.map((point) => point.position)),
    signature(result),
  ]);
  return result;
}
