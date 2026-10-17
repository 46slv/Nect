const MAX_CONTOURS = 10000;
const MAX_POINTS = 100000;
const MAX_AMPLITUDE = 100000;
const MIN_WAVELENGTH = 0.0001;
const MAX_WAVELENGTH = 100000;
const MAX_PHASE = 360;
const MAX_COORDINATE_RATIO = 1e12;

const finiteVector = (value) =>
  Array.isArray(value) && value.length === 2 && value.every(Number.isFinite);

function validateSource(source) {
  if (!source || typeof source !== 'object' || !Array.isArray(source.contours))
    throw Error('Connect a Path input');
  if (source.contours.length > MAX_CONTOURS) throw Error('Wave Points exceeds geometry budget');

  let pointCount = 0;
  for (const contour of source.contours) {
    if (
      !contour ||
      typeof contour !== 'object' ||
      typeof contour.id !== 'string' ||
      !contour.id ||
      typeof contour.closed !== 'boolean'
    )
      throw Error('Wave Points needs valid contours with stable IDs');
    if (!Array.isArray(contour.points)) throw Error('Wave Points needs contour points');
    pointCount += contour.points.length;
    if (pointCount > MAX_POINTS) throw Error('Wave Points exceeds geometry budget');
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
        throw Error('Wave Points needs stable point IDs and finite positions and handles');
    }
  }
  return pointCount;
}

function validateOptions(options) {
  if (!options || typeof options !== 'object' || Array.isArray(options))
    throw Error('Wave Points needs valid options');

  const { amplitude, wavelength, phase, direction, keepEndpoints = false } = options;
  if (!Number.isFinite(amplitude) || Math.abs(amplitude) > MAX_AMPLITUDE)
    throw Error('Wave Points amplitude must be finite and between -100000 and 100000');
  if (!Number.isFinite(wavelength) || wavelength < MIN_WAVELENGTH || wavelength > MAX_WAVELENGTH)
    throw Error('Wave Points wavelength must be finite and between 0.0001 and 100000');
  if (!Number.isFinite(phase) || Math.abs(phase) > MAX_PHASE)
    throw Error('Wave Points phase must be finite and between -360 and 360 degrees');
  if (direction !== 'vertical' && direction !== 'horizontal')
    throw Error('Wave Points direction must be vertical or horizontal');
  if (typeof keepEndpoints !== 'boolean') throw Error('Wave Points keepEndpoints must be boolean');

  const prefix = Object.hasOwn(options, 'prefix') ? options.prefix : 'wave';
  if (typeof prefix !== 'string' || !prefix) throw Error('Wave Points needs a nonempty ID prefix');
  return { amplitude, wavelength, phase, direction, keepEndpoints, prefix };
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

function waveAngle(position, options) {
  const coordinate = options.direction === 'vertical' ? position[0] : position[1];
  const ratio = coordinate / options.wavelength;
  if (!Number.isFinite(ratio) || Math.abs(ratio) > MAX_COORDINATE_RATIO)
    throw Error(
      'Wave Points exceeds the coordinate precision limit (coordinate/wavelength must be within ±1e12)',
    );

  // Reduce spatial turns and phase before evaluating sine to keep its argument small.
  let spatialRemainder = ratio % 1;
  if (spatialRemainder < 0) spatialRemainder += 1;
  const phaseDegrees = options.phase % 360;
  const angle = 2 * Math.PI * spatialRemainder + (phaseDegrees * Math.PI) / 180;
  if (!Number.isFinite(angle)) throw Error('Wave Points exceeds finite-coordinate arithmetic');
  return angle;
}

function displacement(angle, options) {
  const amount = options.amplitude * Math.sin(angle);
  if (!Number.isFinite(amount)) throw Error('Wave Points exceeds finite-coordinate arithmetic');
  return amount;
}

/**
 * Apply a static sine displacement to path anchors. Vertical mode offsets Y
 * from X; horizontal mode offsets X from Y. Relative Bezier handles remain
 * unchanged and translate with their anchors, so this is not an exact
 * continuous sine warp of Bezier interiors. Resample Path first for finer
 * point density. This modifier is static and does not animate over time.
 */
export function waveGeometry(source, options = {}) {
  const normalized = validateOptions(options);
  validateSource(source);
  if (normalized.amplitude === 0) return cloneGeometry(source);

  const contours = source.contours.map((contour, contourIndex) => ({
    ...contour,
    id: `${normalized.prefix}:c${contourIndex}`,
    points: contour.points.map((point, pointIndex) => {
      const angle = waveAngle(point.position, normalized);
      const pinned =
        normalized.keepEndpoints &&
        !contour.closed &&
        (pointIndex === 0 || pointIndex === contour.points.length - 1);
      const amount = pinned ? 0 : displacement(angle, normalized);
      const position = [...point.position];
      const displacedAxis = normalized.direction === 'vertical' ? 1 : 0;
      position[displacedAxis] += amount;
      if (!position.every(Number.isFinite))
        throw Error('Wave Points exceeds finite-coordinate arithmetic');

      return {
        ...point,
        id: `${normalized.prefix}:c${contourIndex}:p${pointIndex}`,
        position,
        handleA: [...point.handleA],
        handleB: [...point.handleB],
      };
    }),
  }));
  return { ...source, contours };
}
