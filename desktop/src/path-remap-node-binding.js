import { isPaintedGroup } from './composition-reference.js';
import { routeCharacters } from './character-geometry.js';

export function evaluatePathRemap(_operation, read, context) {
  const source = read('path');
  if (!source) throw Error('Connect a Path input');
  const result = context.cloneEvaluated(source);
  if (isPaintedGroup(result))
    throw Error('Edit paint in the source Composition, or use Convert to Path');
  result.characterOccurrences = routeCharacters(
    result,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Path Remap',
  );
  const offset = read('offset');
  for (const contour of result.contours)
    for (const point of contour.points)
      point.position = point.position.map((value, i) => value + offset[i]);
  context.forEachPointEdit((edit) => {
    const point = result.contours
      .flatMap((contour) => contour.points)
      .find((p) => p.id === edit.pointId);
    if (
      !point ||
      (result.unstable && !context.identity.matchesFingerprint(result, edit.fingerprint))
    ) {
      result.remapSuspended = true;
      context.warn('vertex edit suspended because its source changed. Rebind explicitly.');
      return;
    }
    for (const key of ['position', 'handleA', 'handleB']) {
      const delta = edit.hasParameter(key)
        ? read(`remap.${edit.pointId}.${key}`)
        : edit.directValue(key);
      if (delta) point[key] = point[key].map((value, i) => value + delta[i]);
    }
  });
  return result;
}
