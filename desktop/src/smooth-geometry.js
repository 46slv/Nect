const MAX_CONTOURS = 10000;
const MAX_POINTS = 100000;
const MAX_POINT_ITERATIONS = 2000000;

const finiteVector = (value) =>
  Array.isArray(value) && value.length === 2 && value.every(Number.isFinite);

function validateSource(source) {
  if (!source || !Array.isArray(source.contours)) throw Error('Connect a Path input');
  if (source.contours.length > MAX_CONTOURS) throw Error('Smooth exceeds geometry budget');

  let pointCount = 0;
  for (const contour of source.contours) {
    if (
      !contour ||
      typeof contour !== 'object' ||
      typeof contour.id !== 'string' ||
      !contour.id ||
      typeof contour.closed !== 'boolean'
    )
      throw Error('Smooth needs valid contours with stable IDs');
    if (!Array.isArray(contour.points)) throw Error('Smooth needs contour points');
    pointCount += contour.points.length;
    if (pointCount > MAX_POINTS) throw Error('Smooth exceeds geometry budget');
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
        throw Error('Smooth needs stable point IDs and finite positions and handles');
    }
  }
  return pointCount;
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

// Interpolate without forming an overflowing difference for opposite-sign
// values. In either branch the mathematical result stays between the inputs.
function weightedAverage(from, to, amount) {
  if (amount === 0) return from;
  if (amount === 1) return to;
  if ((from < 0 && to > 0) || (from > 0 && to < 0)) return from * (1 - amount) + to * amount;
  return from + (to - from) * amount;
}

function average(a, b) {
  return [weightedAverage(a[0], b[0], 0.5), weightedAverage(a[1], b[1], 0.5)];
}

/**
 * Smooth 2D paths by repeatedly moving each point toward the average of its
 * connected neighbors. Each iteration is Jacobi-style: all positions in a
 * pass are calculated from the same prior-pass snapshot. Endpoints of open
 * contours stay fixed by default; Bezier handles remain relative and unchanged.
 *
 * This operation can shrink or collapse shapes. It does not fit Bezier curves
 * or repair topology; it is most useful after Resample and Roughen.
 */
export function smoothGeometry(source, options = {}) {
  if (!options || typeof options !== 'object') throw Error('Smooth needs valid options');
  const { factor, iterations = 1, keepEndpoints = true, prefix = 'smooth' } = options;
  if (!Number.isFinite(factor) || factor < 0 || factor > 1)
    throw Error('Smooth factor must be finite and between 0 and 1');
  if (!Number.isInteger(iterations) || iterations < 1 || iterations > 50)
    throw Error('Smooth iterations must be an integer between 1 and 50');
  if (typeof keepEndpoints !== 'boolean') throw Error('Smooth keepEndpoints must be boolean');
  if (typeof prefix !== 'string' || !prefix) throw Error('Smooth needs a nonempty ID prefix');

  const pointCount = validateSource(source);
  if (factor === 0) return cloneGeometry(source);
  if (pointCount * iterations > MAX_POINT_ITERATIONS)
    throw Error('Smooth exceeds point-iteration work budget');

  const positions = source.contours.map((contour) =>
    contour.points.map((point) => [...point.position]),
  );

  for (let iteration = 0; iteration < iterations; iteration++) {
    const previous = positions;
    const next = source.contours.map((contour, contourIndex) => {
      const points = contour.points;
      const current = previous[contourIndex];
      const count = points.length;
      return current.map((position, pointIndex) => {
        if (count < 2) return [...position];

        const endpoint = !contour.closed && (pointIndex === 0 || pointIndex === count - 1);
        if (endpoint && keepEndpoints) return [...position];

        let target;
        if (!contour.closed && pointIndex === 0) target = current[1];
        else if (!contour.closed && pointIndex === count - 1) target = current[count - 2];
        else {
          const before = current[(pointIndex - 1 + count) % count];
          const after = current[(pointIndex + 1) % count];
          target = average(before, after);
        }

        const smoothed = [
          weightedAverage(position[0], target[0], factor),
          weightedAverage(position[1], target[1], factor),
        ];
        if (!smoothed.every(Number.isFinite)) throw Error('Smooth exceeds finite coordinates');
        return smoothed;
      });
    });
    positions.splice(0, positions.length, ...next);
  }

  return {
    ...source,
    contours: source.contours.map((contour, contourIndex) => ({
      ...contour,
      id: `${prefix}:c${contourIndex}`,
      points: contour.points.map((point, pointIndex) => ({
        ...point,
        id: `${prefix}:c${contourIndex}:p${pointIndex}`,
        position: positions[contourIndex][pointIndex],
        handleA: [...point.handleA],
        handleB: [...point.handleB],
      })),
    })),
  };
}
