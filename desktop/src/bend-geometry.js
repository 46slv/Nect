const MAX_CONTOURS = 10000;
const MAX_POINTS = 100000;
const MAX_ANGLE = 180;
const SMALL_ANGLE = 1e-4;

const finiteVector = (value) =>
  Array.isArray(value) && value.length === 2 && value.every(Number.isFinite);

function validateSource(source) {
  if (!source || typeof source !== 'object' || !Array.isArray(source.contours))
    throw Error('Connect a Path input');
  if (source.contours.length > MAX_CONTOURS) throw Error('Bend Points exceeds geometry budget');

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
      throw Error('Bend Points needs valid contours with stable IDs');
    if (!Array.isArray(contour.points)) throw Error('Bend Points needs contour points');
    pointCount += contour.points.length;
    if (pointCount > MAX_POINTS) throw Error('Bend Points exceeds geometry budget');
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
        throw Error('Bend Points needs stable point IDs and finite positions and handles');

      include(point.position);
      for (const handle of [point.handleA, point.handleB]) {
        const absolute = point.position.map((value, axis) => value + handle[axis]);
        if (!absolute.every(Number.isFinite))
          throw Error('Bend Points exceeds finite-coordinate arithmetic');
        include(absolute);
      }
    }
  }
  return { pointCount, bounds };
}

function validateOptions(options) {
  if (!options || typeof options !== 'object' || Array.isArray(options))
    throw Error('Bend Points needs valid options');

  const { angle = 0, axis = 'horizontal' } = options;
  if (!Number.isFinite(angle) || angle < -MAX_ANGLE || angle > MAX_ANGLE)
    throw Error('Bend Points angle must be finite and between -180 and 180 degrees');
  if (axis !== 'horizontal' && axis !== 'vertical')
    throw Error('Bend Points axis must be horizontal or vertical');

  const prefix = Object.hasOwn(options, 'prefix') ? options.prefix : 'bend';
  if (typeof prefix !== 'string' || !prefix) throw Error('Bend Points needs a nonempty ID prefix');
  return { angle, axis, theta: (angle * Math.PI) / 180, prefix };
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

function midpoint(minimum, maximum) {
  const span = maximum - minimum;
  const value = Number.isFinite(span) ? minimum + span / 2 : minimum / 2 + maximum / 2;
  if (!Number.isFinite(value)) throw Error('Bend Points exceeds finite-coordinate arithmetic');
  return value;
}

function finite(value) {
  if (!Number.isFinite(value)) throw Error('Bend Points exceeds finite-coordinate arithmetic');
  return value;
}

function foldFactor(theta, offset, span) {
  const ratio = finite(offset / span);
  return finite(1 - finite(theta * ratio));
}

function sincAndCosc(phi) {
  const square = phi * phi;
  if (Math.abs(phi) < SMALL_ANGLE) {
    const fourth = square * square;
    const sixth = fourth * square;
    return {
      sinc: 1 - square / 6 + fourth / 120,
      cosc: phi / 2 - (phi * square) / 24 + (phi * fourth) / 720,
    };
  }
  return { sinc: Math.sin(phi) / phi, cosc: (1 - Math.cos(phi)) / phi };
}

function mapPosition(position, origin, axialAxis, crossAxis, span, theta) {
  const q = finite(position[axialAxis] - origin[axialAxis]);
  const w = finite(position[crossAxis] - origin[crossAxis]);
  const phi = finite(theta * finite(q / span));
  const { sinc, cosc } = sincAndCosc(phi);
  const axial = finite(finite(q * sinc) - finite(w * Math.sin(phi)));
  const cross = finite(finite(q * cosc) + finite(w * Math.cos(phi)));
  const mapped = [...position];
  mapped[axialAxis] = finite(origin[axialAxis] + axial);
  mapped[crossAxis] = finite(origin[crossAxis] + cross);
  return mapped;
}

/**
 * Bend every control point around the shared control-bounds midpoint to form a
 * circular strip. Anchors and absolute Bézier controls are mapped separately,
 * so this is a control-point approximation, not an exact nonlinear cubic warp.
 */
export function bendGeometry(source, options = {}) {
  const normalizedOptions = validateOptions(options);
  const { pointCount, bounds } = validateSource(source);
  if (pointCount === 0 || normalizedOptions.angle === 0) return cloneGeometry(source);

  const axialAxis = normalizedOptions.axis === 'horizontal' ? 0 : 1;
  const crossAxis = 1 - axialAxis;
  const [minimum, maximum] = bounds[axialAxis];
  const span = finite(maximum - minimum);
  if (span === 0) return cloneGeometry(source);
  const origin = [midpoint(bounds[0][0], bounds[0][1]), midpoint(bounds[1][0], bounds[1][1])];
  const { theta } = normalizedOptions;
  const minCrossOffset = finite(bounds[crossAxis][0] - origin[crossAxis]);
  const maxCrossOffset = finite(bounds[crossAxis][1] - origin[crossAxis]);
  if (foldFactor(theta, minCrossOffset, span) <= 0 || foldFactor(theta, maxCrossOffset, span) <= 0)
    throw Error('Bend Points would fold the control bounds');

  const contours = source.contours.map((contour, contourIndex) => ({
    ...contour,
    id: `${normalizedOptions.prefix}:c${contourIndex}`,
    points: contour.points.map((point, pointIndex) => {
      const absoluteHandleA = point.position.map((value, axis) => value + point.handleA[axis]);
      const absoluteHandleB = point.position.map((value, axis) => value + point.handleB[axis]);
      const position = mapPosition(point.position, origin, axialAxis, crossAxis, span, theta);
      const mappedHandleA = mapPosition(absoluteHandleA, origin, axialAxis, crossAxis, span, theta);
      const mappedHandleB = mapPosition(absoluteHandleB, origin, axialAxis, crossAxis, span, theta);
      const handleA = mappedHandleA.map((value, axis) => finite(value - position[axis]));
      const handleB = mappedHandleB.map((value, axis) => finite(value - position[axis]));

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
