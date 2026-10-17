const MAX_CONTOURS = 10000;
const MAX_POINTS = 100000;

const finiteVector = (value) =>
  Array.isArray(value) && value.length === 2 && value.every(Number.isFinite);

function validateSource(source) {
  if (
    !source ||
    typeof source !== 'object' ||
    Array.isArray(source) ||
    !Array.isArray(source.contours)
  )
    throw Error('Connect a Path input');
  if (source.contours.length > MAX_CONTOURS) throw Error('Auto Handles exceeds geometry budget');

  let pointCount = 0;
  for (const contour of source.contours) {
    if (
      !contour ||
      typeof contour !== 'object' ||
      Array.isArray(contour) ||
      typeof contour.id !== 'string' ||
      !contour.id ||
      typeof contour.closed !== 'boolean'
    )
      throw Error('Auto Handles needs valid contours with stable IDs');
    if (!Array.isArray(contour.points)) throw Error('Auto Handles needs contour points');
    pointCount += contour.points.length;
    if (pointCount > MAX_POINTS) throw Error('Auto Handles exceeds geometry budget');

    for (const point of contour.points) {
      if (
        !point ||
        typeof point !== 'object' ||
        Array.isArray(point) ||
        typeof point.id !== 'string' ||
        !point.id ||
        !finiteVector(point.position) ||
        !finiteVector(point.handleA) ||
        !finiteVector(point.handleB)
      )
        throw Error('Auto Handles needs stable point IDs and finite positions and handles');
    }
  }

  return pointCount;
}

function validateOptions(options) {
  if (!options || typeof options !== 'object' || Array.isArray(options))
    throw Error('Auto Handles needs valid options');

  const { strength = 1 } = options;
  if (!Number.isFinite(strength) || strength < 0 || strength > 1)
    throw Error('Auto Handles strength must be finite and between 0 and 1');

  const prefix = Object.hasOwn(options, 'prefix') ? options.prefix : 'auto-handles';
  if (typeof prefix !== 'string' || !prefix) throw Error('Auto Handles needs a nonempty ID prefix');
  return { strength, prefix };
}

// Keep ordinary differences exact when possible. If opposite-sign finite
// endpoints overflow during subtraction, halve both operands and carry a
// scale factor so subsequent normalization and length calculations stay safe.
function difference(to, from) {
  const direct = [to[0] - from[0], to[1] - from[1]];
  if (direct.every(Number.isFinite)) return { vector: direct, factor: 1 };

  const halved = [to[0] / 2 - from[0] / 2, to[1] / 2 - from[1] / 2];
  if (!halved.every(Number.isFinite))
    throw Error('Auto Handles exceeds finite-coordinate arithmetic');
  return { vector: halved, factor: 2 };
}

function magnitude(vector) {
  const base = Math.max(Math.abs(vector.vector[0]), Math.abs(vector.vector[1]));
  if (base === 0) return { base: 0, multiplier: 0 };
  const x = vector.vector[0] / base;
  const y = vector.vector[1] / base;
  return { base, multiplier: vector.factor * Math.hypot(x, y) };
}

function compareMagnitudes(a, b) {
  const base = Math.max(a.base, b.base);
  if (base === 0) return 0;
  const scaledA = (a.base / base) * a.multiplier;
  const scaledB = (b.base / base) * b.multiplier;
  return scaledA - scaledB;
}

function lengthAtFraction(scaledMagnitude, fraction) {
  if (scaledMagnitude.base === 0 || fraction === 0) return 0;
  // multiplier is at most 2 * sqrt(2); with fraction <= 1/3 the product
  // is below 1, even when the true chord length exceeds Number.MAX_VALUE.
  const length = scaledMagnitude.base * (scaledMagnitude.multiplier * fraction);
  if (!Number.isFinite(length)) throw Error('Auto Handles exceeds finite-coordinate arithmetic');
  return length;
}

function unitDirection(vector) {
  const base = Math.max(Math.abs(vector.vector[0]), Math.abs(vector.vector[1]));
  if (base === 0) return null;
  const x = vector.vector[0] / base;
  const y = vector.vector[1] / base;
  const length = Math.hypot(x, y);
  if (length === 0 || !Number.isFinite(length)) return null;
  return [x / length, y / length];
}

function scaledDirection(direction, length, sign = 1) {
  return direction.map((component) => {
    const value = component * length * sign;
    if (!Number.isFinite(value)) throw Error('Auto Handles exceeds finite-coordinate arithmetic');
    return value === 0 ? 0 : value;
  });
}

function zeroHandles() {
  return { handleA: [0, 0], handleB: [0, 0] };
}

function openEndpointHandles(delta, strength, isFirst) {
  const direction = unitDirection(delta);
  if (!direction || strength === 0) return zeroHandles();
  const length = lengthAtFraction(magnitude(delta), strength / 3);
  if (length === 0) return zeroHandles();
  const handle = scaledDirection(direction, length);
  return isFirst ? { handleA: [0, 0], handleB: handle } : { handleA: handle, handleB: [0, 0] };
}

function pairedHandles(points, previousIndex, pointIndex, nextIndex, strength) {
  if (strength === 0) return zeroHandles();

  const current = points[pointIndex].position;
  const previous = points[previousIndex].position;
  const next = points[nextIndex].position;
  const incomingChord = magnitude(difference(previous, current));
  const outgoingChord = magnitude(difference(next, current));
  if (incomingChord.base === 0 || outgoingChord.base === 0) return zeroHandles();

  const direction = unitDirection(difference(next, previous));
  if (!direction) return zeroHandles();

  const shortestChord =
    compareMagnitudes(incomingChord, outgoingChord) <= 0 ? incomingChord : outgoingChord;
  const length = lengthAtFraction(shortestChord, strength / 3);
  if (length === 0) return zeroHandles();
  return {
    handleA: scaledDirection(direction, length, -1),
    handleB: scaledDirection(direction, length),
  };
}

function handlesForContour(contour, strength) {
  const points = contour.points;
  const count = points.length;
  if (count < 2) return points.map(zeroHandles);

  return points.map((point, index) => {
    if (!contour.closed && index === 0)
      return openEndpointHandles(difference(points[1].position, point.position), strength, true);
    if (!contour.closed && index === count - 1)
      return openEndpointHandles(
        difference(points[index - 1].position, point.position),
        strength,
        false,
      );

    const previousIndex = (index - 1 + count) % count;
    const nextIndex = (index + 1) % count;
    return pairedHandles(points, previousIndex, index, nextIndex, strength);
  });
}

function cloneGeometry(source) {
  try {
    return structuredClone(source);
  } catch {
    throw Error('Auto Handles needs cloneable geometry metadata');
  }
}

function validateOutputHandles(point, { handleA, handleB }) {
  if (!finiteVector(handleA) || !finiteVector(handleB))
    throw Error('Auto Handles exceeds finite-coordinate arithmetic');
  for (let axis = 0; axis < 2; axis++) {
    if (
      !Number.isFinite(point.position[axis] + handleA[axis]) ||
      !Number.isFinite(point.position[axis] + handleB[axis])
    )
      throw Error('Auto Handles exceeds finite absolute handle endpoints');
  }
}

/**
 * Recompute automatic, paired Bezier handles from the anchor geometry without
 * moving, adding, removing, or reordering anchors. Open contour endpoints get
 * only their one connected handle; closed contours wrap their neighbors.
 *
 * This local tangent construction is a useful smooth-handle approximation,
 * not exact Blender parity and not a topology-preserving curve fit. Acute turns
 * can overshoot or self-intersect.
 */
export function autoHandlesGeometry(source, options = {}) {
  const { strength, prefix } = validateOptions(options);
  validateSource(source);
  const output = cloneGeometry(source);

  output.contours = source.contours.map((contour, contourIndex) => {
    const handles = handlesForContour(contour, strength);
    return {
      ...output.contours[contourIndex],
      id: `${prefix}:c${contourIndex}`,
      points: contour.points.map((point, pointIndex) => {
        const nextPoint = {
          ...output.contours[contourIndex].points[pointIndex],
          id: `${prefix}:c${contourIndex}:p${pointIndex}`,
          handleA: handles[pointIndex].handleA,
          handleB: handles[pointIndex].handleB,
        };
        validateOutputHandles(point, handles[pointIndex]);
        return nextPoint;
      }),
    };
  });

  return output;
}
