const MAX_CONTOURS = 10000;
const MAX_POINTS = 100000;
const MIN_AMOUNT = -50;
const MAX_AMOUNT = 50;

const finiteVector = (value) =>
  Array.isArray(value) && value.length === 2 && value.every(Number.isFinite);

function validateSource(source) {
  if (!source || typeof source !== 'object' || !Array.isArray(source.contours))
    throw Error('Connect a Path input');
  if (source.contours.length > MAX_CONTOURS) throw Error('Taper Points exceeds geometry budget');

  let pointCount = 0;
  for (const contour of source.contours) {
    if (
      !contour ||
      typeof contour !== 'object' ||
      typeof contour.id !== 'string' ||
      !contour.id ||
      typeof contour.closed !== 'boolean'
    )
      throw Error('Taper Points needs valid contours with stable IDs');
    if (!Array.isArray(contour.points)) throw Error('Taper Points needs contour points');
    pointCount += contour.points.length;
    if (pointCount > MAX_POINTS) throw Error('Taper Points exceeds geometry budget');
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
        throw Error('Taper Points needs stable point IDs and finite positions and handles');
      if (
        !point.position.every(
          (value, axis) =>
            Number.isFinite(value + point.handleA[axis]) &&
            Number.isFinite(value + point.handleB[axis]),
        )
      )
        throw Error('Taper Points exceeds finite-coordinate arithmetic');
    }
  }
  return pointCount;
}

function validateOptions(options) {
  if (!options || typeof options !== 'object' || Array.isArray(options))
    throw Error('Taper Points needs valid options');

  const { amount = 0, axis = 'horizontal' } = options;
  if (!Number.isFinite(amount) || amount < MIN_AMOUNT || amount > MAX_AMOUNT)
    throw Error('Taper Points amount must be finite and between -50 and 50 percent');
  if (axis !== 'horizontal' && axis !== 'vertical')
    throw Error('Taper Points axis must be horizontal or vertical');

  const prefix = Object.hasOwn(options, 'prefix') ? options.prefix : 'taper';
  if (typeof prefix !== 'string' || !prefix) throw Error('Taper Points needs a nonempty ID prefix');
  return { amount, axis, prefix };
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

function collectBounds(source) {
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

  for (const contour of source.contours) {
    for (const point of contour.points) {
      include(point.position);
      include(point.position.map((value, axis) => value + point.handleA[axis]));
      include(point.position.map((value, axis) => value + point.handleB[axis]));
    }
  }
  return bounds;
}

function normalizedCoordinate(value, minimum, maximum) {
  const magnitude = Math.max(Math.abs(value), Math.abs(minimum), Math.abs(maximum));
  const scaledValue = value / magnitude;
  const scaledMinimum = minimum / magnitude;
  const scaledMaximum = maximum / magnitude;
  const fraction = (scaledValue - scaledMinimum) / (scaledMaximum - scaledMinimum);
  return Math.max(-1, Math.min(1, 2 * fraction - 1));
}

function midpoint(minimum, maximum) {
  const span = maximum - minimum;
  return Number.isFinite(span) ? minimum + span / 2 : minimum / 2 + maximum / 2;
}

function scaleAboutMidpoint(value, minimum, maximum, scale) {
  const center = midpoint(minimum, maximum);
  const direct = center + (value - center) * scale;
  if (Number.isFinite(direct)) return direct;

  // Normalize before arithmetic when a valid transformed coordinate would
  // otherwise overflow in an intermediate subtraction or multiplication.
  const magnitude = Math.max(Math.abs(value), Math.abs(minimum), Math.abs(maximum));
  const normalizedCenter = midpoint(minimum / magnitude, maximum / magnitude);
  const normalizedValue = value / magnitude;
  const normalizedResult = normalizedCenter + (normalizedValue - normalizedCenter) * scale;
  const result = normalizedResult * magnitude;
  if (Number.isFinite(result)) return result;
  throw Error('Taper Points exceeds finite-coordinate arithmetic');
}

function mapAbsolutePosition(position, bounds, normalizedOptions) {
  const { amount, axis } = normalizedOptions;
  const deformAxis = axis === 'horizontal' ? 0 : 1;
  const scaledAxis = 1 - deformAxis;
  const [minimum, maximum] = bounds[deformAxis];
  const u = normalizedCoordinate(position[deformAxis], minimum, maximum);
  const scale = 1 + (amount / 100) * u;
  const mapped = [...position];
  mapped[scaledAxis] = scaleAboutMidpoint(
    position[scaledAxis],
    bounds[scaledAxis][0],
    bounds[scaledAxis][1],
    scale,
  );
  if (!mapped.every(Number.isFinite))
    throw Error('Taper Points exceeds finite-coordinate arithmetic');
  return mapped;
}

/**
 * Taper a set of 2D contours by scaling their perpendicular coordinate around
 * the whole input's bounds midpoint. Bounds include anchors and absolute
 * Bezier control endpoints, so every contour shares the same coordinate frame.
 * This maps anchors and handles independently as a control-point approximation;
 * it does not exactly warp nonlinear curve interiors.
 */
export function taperGeometry(source, options = {}) {
  const normalizedOptions = validateOptions(options);
  const pointCount = validateSource(source);
  if (pointCount === 0 || normalizedOptions.amount === 0) return cloneGeometry(source);

  const bounds = collectBounds(source);
  const deformAxis = normalizedOptions.axis === 'horizontal' ? 0 : 1;
  if (bounds[deformAxis][0] === bounds[deformAxis][1]) return cloneGeometry(source);

  const contours = source.contours.map((contour, contourIndex) => ({
    ...contour,
    id: `${normalizedOptions.prefix}:c${contourIndex}`,
    points: contour.points.map((point, pointIndex) => {
      const absoluteHandleA = point.position.map((value, axis) => value + point.handleA[axis]);
      const absoluteHandleB = point.position.map((value, axis) => value + point.handleB[axis]);
      const position = mapAbsolutePosition(point.position, bounds, normalizedOptions);
      const mappedHandleA = mapAbsolutePosition(absoluteHandleA, bounds, normalizedOptions);
      const mappedHandleB = mapAbsolutePosition(absoluteHandleB, bounds, normalizedOptions);
      const handleA = mappedHandleA.map((value, axis) => value - position[axis]);
      const handleB = mappedHandleB.map((value, axis) => value - position[axis]);
      if (![...handleA, ...handleB].every(Number.isFinite))
        throw Error('Taper Points exceeds finite-coordinate arithmetic');

      return {
        ...point,
        id: `${normalizedOptions.prefix}:c${contourIndex}:p${pointIndex}`,
        position,
        handleA,
        handleB,
      };
    }),
  }));

  return { ...source, contours };
}
