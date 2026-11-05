import { contourGroups } from './path-groups.js';
import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';

export function evaluateJoinGeometry(_operation, read, context) {
  const slots = context.shape.slots.filter((slot) => slot.connected);
  if (!slots.length) throw Error('Connect a path to Join Geometry');
  if (context.shape.mode === 'painted')
    return context.paintedGroup(slots.map((slot) => read(slot.id)));
  const inputs = slots.map((slot) => [slot.id, read(slot.id)]);
  let contourCount = 0,
    pointCount = 0;
  const groups = [];
  for (const [, path] of inputs) {
    contourCount += path.contours.length;
    for (const contour of path.contours) pointCount += contour.points.length;
    if (contourCount > 10000 || pointCount > 100000) throw Error('Join exceeds geometry budget');
    const inputGroups = contourGroups(path);
    if (groups.length + inputGroups.length > 256)
      throw Error('Join exceeds 256 independent groups');
    groups.push(...inputGroups);
  }
  const prefix = fingerprint(context.nodeId);
  const result = {
    contours: groups.flatMap((group, gi) =>
      group.map((c, ci) => ({
        ...structuredClone(c),
        id: `${prefix}:g${gi}:c${ci}`,
        group: `${prefix}:g${gi}`,
        points: c.points.map((pt, pi) => ({
          ...structuredClone(pt),
          id: `${prefix}:g${gi}:c${ci}:p${pi}`,
        })),
      })),
    ),
  };
  result.characterOccurrences = inputs.flatMap(([key, path]) =>
    routeCharacters(path, `${context.nodeId}:${key}`),
  );
  result.unstable = true;
  result.fingerprint = fingerprint(signature(result));
  return result;
}
