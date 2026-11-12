import { toPaper } from './paper-runtime.js';
import { isVector2 } from './node-value-validation.js';
import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';

export function evaluateResamplePath(operation, read, context) {
  const source = read('path');
  if (!source) throw Error('Connect a Path input');
  if (!['count', 'spacing'].includes(operation)) throw Error('Invalid Resample operation');
  const count = operation === 'count' ? read('count') : null,
    spacing = operation === 'spacing' ? read('spacing') : null,
    sourcePoints = source.contours.reduce((sum, c) => sum + c.points.length, 0);
  if (operation === 'count') {
    if (!Number.isInteger(count) || count < 2 || count > 1024)
      throw Error('Resample count must be an integer from 2 to 1024');
    if (count < 3 && source.contours.some((c) => c.closed))
      throw Error('Closed contours require at least 3 samples');
  } else if (!Number.isFinite(spacing) || spacing < 0.0001 || spacing > 100000)
    throw Error('Resample spacing must be from 0.0001 to 100000 px');
  if (
    source.contours.length > 10000 ||
    sourcePoints > 100000 ||
    (operation === 'count' &&
      (source.contours.length * count > 100000 || sourcePoints * count > 2000000))
  )
    throw Error('Resample exceeds geometry budget');
  const item = toPaper(source),
    prefix = fingerprint(context.nodeId);
  let result;
  try {
    const plans = source.contours.map((c, ci) => {
      const path = item.children[ci],
        length = path.length;
      if (!Number.isFinite(length)) throw Error('Resample exceeds finite path length');
      const samples =
        operation === 'count'
          ? count
          : c.points.length === 0
            ? 0
            : c.closed
              ? Math.max(3, Math.ceil(length / spacing))
              : Math.max(2, Math.ceil(length / spacing) + 1);
      if (
        c.points.length > 0 &&
        (!Number.isInteger(samples) || samples < (c.closed ? 3 : 2) || samples > 1024)
      )
        throw Error('Resample spacing requires more than 1024 samples for a contour');
      return { path, length, samples };
    });
    const outputPoints = plans.reduce((sum, plan) => sum + plan.samples, 0),
      work = plans.reduce(
        (sum, plan, index) => sum + source.contours[index].points.length * plan.samples,
        0,
      );
    if (outputPoints > 100000 || work > 2000000) throw Error('Resample exceeds geometry budget');
    result = {
      contours: source.contours.map((c, ci) => {
        const { path, length, samples } = plans[ci];
        return {
          id: `${prefix}:c${ci}`,
          closed: c.closed,
          ...(c.group !== undefined ? { group: c.group } : {}),
          points: c.points.length
            ? Array.from({ length: samples }, (_, pi) => {
                const distance = length * (pi / (c.closed ? samples : samples - 1));
                const sampled = path.getPointAt(distance);
                const position = sampled
                  ? [sampled.x, sampled.y]
                  : length === 0
                    ? [...c.points[0].position]
                    : null;
                if (!isVector2(position)) throw Error('Expected Vector2');
                return {
                  id: `${prefix}:c${ci}:p${pi}`,
                  position,
                  handleA: [0, 0],
                  handleB: [0, 0],
                };
              })
            : [],
        };
      }),
    };
  } finally {
    item.remove();
  }
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Resample Path; edit the source Text',
  );
  result.unstable = true;
  result.fingerprint = fingerprint(signature(result));
  return result;
}
