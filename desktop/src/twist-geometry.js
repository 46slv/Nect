const MAX_CONTOURS = 10000;
const MAX_POINTS = 100000;
const MIN_ANGLE = -360;
const MAX_ANGLE = 360;

const finiteVector = (value) =>
  Array.isArray(value) && value.length === 2 && value.every(Number.isFinite);

function finite(value) {
  if (!Number.isFinite(value)) throw Error('Twist Points exceeds finite-coordinate arithmetic');
  return value;
}

function cloneValue(value) {
  if (Array.isArray(value)) return value.map(cloneValue);
  if (!value || typeof value !== 'object') return value;
  const copy = {};
  for (const [key, item] of Object.entries(value)) copy[key] = cloneValue(item);
  return copy;
}

function cloneGeometry(source) {
  return cloneValue(source);
}

function validateSource(source) {
  if (!source || typeof source !== 'object' || !Array.isArray(source.contours))
    throw Error('Connect a Path input');
  if (source.contours.length > MAX_CONTOURS) throw Error('Twist Points exceeds geometry budget');

  const bounds = [
    [Infinity, -Infinity],
    [Infinity, -Infinity],
  ];
  const include = (position) => {
    for (let axis = 0; axis < 2; axis++) {
      bounds[axis][0] = Math.min(bounds[axis][0], position[axis]);
      bounds[axis][1] = Math.max(bounds[axis][1], position[axis]);
    }
  };

  let pointCount = 0;
  for (const contour of source.contours) {
    if (
      !contour ||
      typeof contour !== 'object' ||
      typeof contour.id !== 'string' ||
      !contour.id ||
      typeof contour.closed !== 'boolean'
    )
      throw Error('Twist Points needs valid contours with stable IDs');
    if (!Array.isArray(contour.points)) throw Error('Twist Points needs contour points');
    pointCount += contour.points.length;
    if (pointCount > MAX_POINTS) throw Error('Twist Points exceeds geometry budget');
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
        throw Error('Twist Points needs stable point IDs and finite positions and handles');

      include(point.position);
      for (const handle of [point.handleA, point.handleB]) {
        const absolute = point.position.map((value, axis) => value + handle[axis]);
        if (!absolute.every(Number.isFinite))
          throw Error('Twist Points exceeds finite-coordinate arithmetic');
        include(absolute);
      }
    }
  }
  return { pointCount, bounds };
}

function validateOptions(options) {
  if (!options || typeof options !== 'object' || Array.isArray(options))
    throw Error('Twist Points needs valid options');

  const { angle = 0 } = options;
  if (!Number.isFinite(angle) || angle < MIN_ANGLE || angle > MAX_ANGLE)
    throw Error('Twist Points angle must be finite and between -360 and 360 degrees');

  const prefix = Object.hasOwn(options, 'prefix') ? options.prefix : 'twist';
  if (typeof prefix !== 'string' || !prefix) throw Error('Twist Points needs a nonempty ID prefix');
  return { angle, prefix };
}

function midpoint(minimum, maximum) {
  const span = maximum - minimum;
  return finite(Number.isFinite(span) ? minimum + span / 2 : minimum / 2 + maximum / 2);
}

// Return half of a span without overflowing when the endpoints straddle the
// largest finite coordinate. The result is the half-width used by R.
function halfSpan(minimum, maximum) {
  const span = maximum - minimum;
  return finite(Number.isFinite(span) ? span / 2 : maximum / 2 - minimum / 2);
}

function normalizedRadius(position, center, scale, scaledRadius) {
  const dx = finite(position[0] - center[0]);
  const dy = finite(position[1] - center[1]);
  const offsetScale = Math.max(Math.abs(dx), Math.abs(dy));
  if (offsetScale === 0) return 0;

  // Compute hypot(dx, dy) / hypot(halfWidth, halfHeight) in a shared scale.
  // This is exactly distance / R for R = 0.5 * hypot(width, height), while
  // avoiding an overflowing full diagonal for extreme but finite inputs.
  const scaledDistance = (offsetScale / scale) * Math.hypot(dx / offsetScale, dy / offsetScale);
  return Math.max(0, Math.min(1, finite(scaledDistance / scaledRadius)));
}

function rotateOffset(dx, dy, angle) {
  const scale = Math.max(Math.abs(dx), Math.abs(dy));
  if (scale === 0) return [0, 0];
  const x = dx / scale;
  const y = dy / scale;
  const cosine = Math.cos(angle);
  const sine = Math.sin(angle);
  // In screen coordinates (Y down), this conventional rotation matrix makes
  // positive angles clockwise. Normalize first so intermediate sums stay safe.
  return [finite((x * cosine - y * sine) * scale), finite((x * sine + y * cosine) * scale)];
}

function mapPosition(position, center, halfWidth, halfHeight, scale, scaledRadius, angle) {
  const u = normalizedRadius(position, center, scale, scaledRadius);
  const smooth = u * u * (3 - 2 * u);
  const turns = angle * (1 - smooth);
  if (turns === 0) return [...position];
  const radians = finite((turns * Math.PI) / 180);
  const offset = rotateOffset(position[0] - center[0], position[1] - center[1], radians);
  return [finite(center[0] + offset[0]), finite(center[1] + offset[1])];
}

/**
 * Rotate anchors and absolute Bézier controls by a shared angle that eases
 * smoothly from the full angle at the bounds midpoint to zero at its control
 * bounds radius. This maps controls independently; it does not claim an exact
 * nonlinear warp of cubic interiors. Resample Path first for finer detail.
 */
export function twistGeometry(source, options = {}) {
  const normalized = validateOptions(options);
  const { pointCount, bounds } = validateSource(source);
  if (pointCount === 0 || normalized.angle === 0) return cloneGeometry(source);

  const center = [midpoint(bounds[0][0], bounds[0][1]), midpoint(bounds[1][0], bounds[1][1])];
  const halfWidth = halfSpan(bounds[0][0], bounds[0][1]);
  const halfHeight = halfSpan(bounds[1][0], bounds[1][1]);
  const scale = Math.max(halfWidth, halfHeight);
  if (scale === 0) return cloneGeometry(source);
  const scaledRadius = Math.hypot(halfWidth / scale, halfHeight / scale);
  if (!Number.isFinite(scaledRadius) || scaledRadius <= 0)
    throw Error('Twist Points exceeds finite-coordinate arithmetic');

  const contours = source.contours.map((contour, contourIndex) => ({
    ...cloneValue(contour),
    id: `${normalized.prefix}:c${contourIndex}`,
    points: contour.points.map((point, pointIndex) => {
      const absoluteHandleA = point.position.map((value, axis) => value + point.handleA[axis]);
      const absoluteHandleB = point.position.map((value, axis) => value + point.handleB[axis]);
      const position = mapPosition(
        point.position,
        center,
        halfWidth,
        halfHeight,
        scale,
        scaledRadius,
        normalized.angle,
      );
      const mappedHandleA = mapPosition(
        absoluteHandleA,
        center,
        halfWidth,
        halfHeight,
        scale,
        scaledRadius,
        normalized.angle,
      );
      const mappedHandleB = mapPosition(
        absoluteHandleB,
        center,
        halfWidth,
        halfHeight,
        scale,
        scaledRadius,
        normalized.angle,
      );
      const handleA = mappedHandleA.map((value, axis) => finite(value - position[axis]));
      const handleB = mappedHandleB.map((value, axis) => finite(value - position[axis]));

      return {
        ...cloneValue(point),
        id: `${normalized.prefix}:c${contourIndex}:p${pointIndex}`,
        position,
        handleA,
        handleB,
      };
    }),
  }));

  return { ...cloneValue(source), contours };
}
