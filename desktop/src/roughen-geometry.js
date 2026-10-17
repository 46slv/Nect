const MAX_CONTOURS = 10000;
const MAX_POINTS = 100000;
const MAX_AMOUNT = 100000;
const MAX_SEED = 2147483647;

const finiteVector = (value) =>
  Array.isArray(value) && value.length === 2 && value.every(Number.isFinite);

function validateSource(source) {
  if (!source || !Array.isArray(source.contours)) throw Error('Connect a Path input');
  if (source.contours.length > MAX_CONTOURS) throw Error('Roughen exceeds geometry budget');

  let pointCount = 0;
  for (const contour of source.contours) {
    if (
      !contour ||
      typeof contour !== 'object' ||
      typeof contour.id !== 'string' ||
      !contour.id ||
      typeof contour.closed !== 'boolean'
    )
      throw Error('Roughen needs valid contours with stable IDs');
    if (!Array.isArray(contour.points)) throw Error('Roughen needs contour points');
    pointCount += contour.points.length;
    if (pointCount > MAX_POINTS) throw Error('Roughen exceeds geometry budget');
    for (const point of contour.points) {
      if (
        !point ||
        typeof point !== 'object' ||
        typeof point.id !== 'string' ||
        !point.id ||
        !finiteVector(point.position) ||
        !finiteVector(point.handleA) ||
        !finiteVector(point.handleB)
      )
        throw Error('Roughen needs stable point IDs and finite positions and handles');
    }
  }
}

// FNV-1a over UTF-16 code units followed by an avalanche mix. Explicit
// separators keep distinct ID tuples from being ambiguous; all operations are
// 32-bit integer operations so results are stable across evaluations.
function pointHash(seed, contourId, pointId, axis) {
  const input = `${seed}\u0000${contourId}\u0000${pointId}\u0000${axis}`;
  let hash = 0x811c9dc5;
  for (let i = 0; i < input.length; i++) {
    hash = Math.imul(hash ^ input.charCodeAt(i), 0x01000193);
  }
  hash ^= hash >>> 16;
  hash = Math.imul(hash, 0x85ebca6b);
  hash ^= hash >>> 13;
  hash = Math.imul(hash, 0xc2b2ae35);
  hash ^= hash >>> 16;
  return hash >>> 0;
}

function offsetFor(amount, seed, contourId, pointId, axis) {
  const unit = pointHash(seed, contourId, pointId, axis) / 0x100000000;
  return (unit * 2 - 1) * amount;
}

function cloneGeometry(source) {
  return {
    ...source,
    contours: source.contours.map((contour) => ({
      ...contour,
      points: contour.points.map((point) => ({
        ...point,
        position: [...point.position],
        handleA: [...point.handleA],
        handleB: [...point.handleB],
      })),
    })),
  };
}

/**
 * Apply a stable, seeded 2D position offset to each path point. Bezier handles
 * remain relative to their translated point, so the curve moves with it.
 */
export function roughenGeometry(source, options = {}) {
  if (!options || typeof options !== 'object') throw Error('Roughen needs valid options');
  const { amount, seed, keepEndpoints = true, prefix = 'roughen' } = options;
  if (!Number.isFinite(amount) || amount < 0 || amount > MAX_AMOUNT)
    throw Error('Roughen amount must be finite and between 0 and 100000');
  if (!Number.isInteger(seed) || seed < 0 || seed > MAX_SEED)
    throw Error('Roughen seed must be an integer between 0 and 2147483647');
  if (typeof keepEndpoints !== 'boolean') throw Error('Roughen keepEndpoints must be boolean');
  if (typeof prefix !== 'string' || !prefix) throw Error('Roughen needs a nonempty ID prefix');
  validateSource(source);
  if (amount === 0) return cloneGeometry(source);

  let pointCount = 0;
  const contours = source.contours.map((contour, contourIndex) => ({
    ...contour,
    id: `${prefix}:c${contourIndex}`,
    points: contour.points.map((point, pointIndex) => {
      pointCount++;
      if (pointCount > MAX_POINTS) throw Error('Roughen exceeds geometry budget');

      const pinned =
        keepEndpoints &&
        !contour.closed &&
        (pointIndex === 0 || pointIndex === contour.points.length - 1);
      const position = pinned
        ? [...point.position]
        : [
            point.position[0] + offsetFor(amount, seed, contour.id, point.id, 0),
            point.position[1] + offsetFor(amount, seed, contour.id, point.id, 1),
          ];
      if (!position.every(Number.isFinite)) throw Error('Roughen exceeds finite coordinates');

      return {
        ...point,
        id: `${prefix}:c${contourIndex}:p${pointIndex}`,
        position,
        handleA: [...point.handleA],
        handleB: [...point.handleB],
      };
    }),
  }));
  return { ...source, contours };
}
