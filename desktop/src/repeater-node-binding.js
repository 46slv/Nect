import { repeatGeometry, repeaterSettings } from './repeater.js';
import { contourGroups } from './path-groups.js';
import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';
import { isVector2 } from './node-value-validation.js';

export function evaluateRepeater(operation, read, context) {
  const source = read('path');
  if (!source) throw Error('Connect a Path input');
  let result;
  if (operation === 'ae-2d') {
    result = repeatGeometry(source, {
      ...repeaterSettings(read),
      prefix: fingerprint(context.nodeId),
    });
  } else {
    const count = read('count');
    if (!Number.isInteger(count) || count < 1 || count > 256)
      throw Error('Repeater count must be an integer from 1 to 256');
    if (
      source.contours.length * count > 10000 ||
      source.contours.reduce((sum, c) => sum + c.points.length, 0) * count > 100000
    )
      throw Error('Repeater exceeds geometry budget');
    const grouped = source.contours.some((c) => c.group !== undefined);
    const groups = grouped
      ? new Map(contourGroups(source).map((group, i) => [group[0].group ?? '', i]))
      : null;
    if (grouped && groups.size * count > 256)
      throw Error('Repeater exceeds 256 independent groups');
    const linear = operation === 'linear';
    if (!['linear', 'radial-fit', 'radial-step', 'ae-2d'].includes(operation))
      throw Error('Invalid repeater operation');
    const step = linear ? read('step') : [0, 0];
    const center = linear ? [0, 0] : read('center');
    const start = linear ? 0 : read('startAngle');
    const angle = linear ? 0 : read('angle');
    if (operation === 'radial-fit' && Math.abs(angle) > 360)
      throw Error('Fit arc must be between -360 and 360 degrees');
    const increment =
      operation === 'radial-fit'
        ? angle / (Math.abs(angle) === 360 ? count : Math.max(1, count - 1))
        : angle;
    result = { contours: [] };
    const prefix = fingerprint(context.nodeId);
    for (let copy = 0; copy < count; copy++) {
      const radians = (((start + increment * copy) % 360) * Math.PI) / 180;
      const cos = Math.cos(radians),
        sin = Math.sin(radians);
      function transform(v, handle = false) {
        const x = v[0] - (handle ? 0 : center[0]),
          y = v[1] - (handle ? 0 : center[1]);
        const next = [
          x * cos - y * sin + (handle ? 0 : center[0] + step[0] * copy),
          x * sin + y * cos + (handle ? 0 : center[1] + step[1] * copy),
        ];
        if (!isVector2(next)) throw Error('Expected Vector2');
        return next;
      }
      for (const [ci, c] of source.contours.entries())
        result.contours.push({
          id: `${prefix}:copy${copy}:c${ci}`,
          ...(grouped ? { group: `${prefix}:copy${copy}:g${groups.get(c.group ?? '')}` } : {}),
          closed: c.closed,
          points: c.points.map((pt, pi) => ({
            id: `${prefix}:copy${copy}:c${ci}:p${pi}`,
            position: transform(pt.position),
            handleA: transform(pt.handleA, true),
            handleB: transform(pt.handleB, true),
          })),
        });
    }
  }
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Geometry Repeater',
  );
  result.unstable = true;
  result.fingerprint = fingerprint(signature(result));
  return result;
}
