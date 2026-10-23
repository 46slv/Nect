const MAX_CONTOURS = 10000;
const MAX_POINTS = 100000;

const finiteVector = (value) =>
  Array.isArray(value) && value.length === 2 && value.every(Number.isFinite);

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

function validateSource(source) {
  if (!source || typeof source !== 'object' || !Array.isArray(source.contours))
    throw Error('Connect a Path input');
  if (source.contours.length > MAX_CONTOURS) throw Error('Pucker & Bloat exceeds geometry budget');

  let pointCount = 0;
  for (const contour of source.contours) {
    if (
      !contour ||
      typeof contour !== 'object' ||
      typeof contour.id !== 'string' ||
      !contour.id ||
      typeof contour.closed !== 'boolean'
    )
      throw Error('Pucker & Bloat needs valid contours with stable IDs');
    if (!Array.isArray(contour.points)) throw Error('Pucker & Bloat needs contour points');
    pointCount += contour.points.length;
    if (pointCount > MAX_POINTS) throw Error('Pucker & Bloat exceeds geometry budget');
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
        throw Error('Pucker & Bloat needs stable point IDs and finite positions and handles');
    }
  }
  return pointCount;
}

function validateOptions(options) {
  if (!options || typeof options !== 'object' || Array.isArray(options))
    throw Error('Pucker & Bloat needs valid options');
  const { amount, autoCenter, center } = options;
  if (!Number.isFinite(amount) || amount < -100 || amount > 100)
    throw Error('Pucker & Bloat amount must be finite and between -100 and 100');
  if (typeof autoCenter !== 'boolean') throw Error('Pucker & Bloat autoCenter must be boolean');
  if (Object.hasOwn(options, 'center') && !finiteVector(center))
    throw Error('Pucker & Bloat center must be a finite Vector2');
  if (!autoCenter && !finiteVector(center))
    throw Error('Pucker & Bloat needs a finite center when autoCenter is false');

  const prefix = Object.hasOwn(options, 'prefix') ? options.prefix : 'pucker-bloat';
  if (typeof prefix !== 'string' || !prefix)
    throw Error('Pucker & Bloat needs a nonempty ID prefix');
  return { amount, autoCenter, center, prefix };
}

function boundingCenter(source, pointCount) {
  if (pointCount === 0) return [0, 0];
  let minX = Infinity,
    minY = Infinity,
    maxX = -Infinity,
    maxY = -Infinity;
  for (const contour of source.contours) {
    for (const point of contour.points) {
      minX = Math.min(minX, point.position[0]);
      minY = Math.min(minY, point.position[1]);
      maxX = Math.max(maxX, point.position[0]);
      maxY = Math.max(maxY, point.position[1]);
    }
  }
  // Halving first avoids overflow when the bounding-box extremes have the same sign.
  return [minX / 2 + maxX / 2, minY / 2 + maxY / 2];
}

function transformAbsolutePoint(point, center, scale) {
  const result = point.map((value, axis) => {
    const offset = value - center[axis];
    const scaledOffset = scale * offset;
    const transformed = center[axis] + scaledOffset;
    if (!Number.isFinite(offset) || !Number.isFinite(scaledOffset) || !Number.isFinite(transformed))
      throw Error('Pucker & Bloat exceeds finite-coordinate arithmetic');
    return transformed;
  });
  return result;
}

/**
 * Apply a static Pucker & Bloat transform to anchors and the absolute endpoints
 * of their relative Bezier handles. Positive amounts scale anchors toward the
 * chosen center and controls away; negative amounts do the reverse. Automatic
 * centering uses the bounding-box center of anchor positions, not exact Bezier
 * extrema or a separate center for each group. The result is deterministic but
 * is not guaranteed to preserve topology or match Adobe Illustrator numerically.
 */
export function puckerBloatGeometry(source, options = {}) {
  const { amount, autoCenter, center: explicitCenter, prefix } = validateOptions(options);
  const pointCount = validateSource(source);
  if (amount === 0) return cloneGeometry(source);

  const center = autoCenter ? boundingCenter(source, pointCount) : explicitCenter;
  const anchorScale = 1 - amount / 100;
  const controlScale = 1 + amount / 100;
  const contours = source.contours.map((contour, contourIndex) => ({
    ...contour,
    id: `${prefix}:c${contourIndex}`,
    points: contour.points.map((point, pointIndex) => {
      const position = transformAbsolutePoint(point.position, center, anchorScale);
      const transformHandle = (handle) => {
        const absoluteControl = [point.position[0] + handle[0], point.position[1] + handle[1]];
        if (!absoluteControl.every(Number.isFinite))
          throw Error('Pucker & Bloat exceeds finite-coordinate arithmetic');
        const transformedControl = transformAbsolutePoint(absoluteControl, center, controlScale);
        const relativeHandle = [
          transformedControl[0] - position[0],
          transformedControl[1] - position[1],
        ];
        if (!relativeHandle.every(Number.isFinite))
          throw Error('Pucker & Bloat exceeds finite-coordinate arithmetic');
        return relativeHandle;
      };

      return {
        ...point,
        id: `${prefix}:c${contourIndex}:p${pointIndex}`,
        position,
        handleA: transformHandle(point.handleA),
        handleB: transformHandle(point.handleB),
      };
    }),
  }));
  return { ...source, contours };
}
