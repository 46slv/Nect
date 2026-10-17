const MAX_CONTOURS = 10000;
const MAX_POINTS = 100000;

const finiteVector = (value) =>
  Array.isArray(value) && value.length === 2 && value.every(Number.isFinite);

function validateSource(source) {
  if (!source || typeof source !== 'object' || !Array.isArray(source.contours))
    throw Error('Connect a Path input');
  if (source.contours.length > MAX_CONTOURS) throw Error('Reverse Path exceeds geometry budget');

  let pointCount = 0;
  for (const contour of source.contours) {
    if (
      !contour ||
      typeof contour !== 'object' ||
      typeof contour.id !== 'string' ||
      !contour.id ||
      typeof contour.closed !== 'boolean'
    )
      throw Error('Reverse Path needs valid contours with stable IDs');
    if (!Array.isArray(contour.points)) throw Error('Reverse Path needs contour points');
    pointCount += contour.points.length;
    if (pointCount > MAX_POINTS) throw Error('Reverse Path exceeds geometry budget');
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
        throw Error('Reverse Path needs stable point IDs and finite positions and handles');
    }
  }
}

function validateOptions(options) {
  if (!options || typeof options !== 'object' || Array.isArray(options))
    throw Error('Reverse Path needs valid options');

  const prefix = Object.hasOwn(options, 'prefix') ? options.prefix : 'reverse';
  if (typeof prefix !== 'string' || !prefix) throw Error('Reverse Path needs a nonempty ID prefix');
  return { prefix };
}

function reversedPoints(contour) {
  if (!contour.closed || contour.points.length < 2) return [...contour.points].reverse();
  return [contour.points[0], ...contour.points.slice(1).reverse()];
}

/** Reverse contour direction while retaining closed-contour seams and exact Bezier geometry. */
export function reversePathGeometry(source, options = {}) {
  const { prefix } = validateOptions(options);
  validateSource(source);

  const contours = source.contours.map((contour, contourIndex) => ({
    ...contour,
    id: `${prefix}:c${contourIndex}`,
    points: reversedPoints(contour).map((point, pointIndex) => ({
      ...point,
      id: `${prefix}:c${contourIndex}:p${pointIndex}`,
      position: [...point.position],
      handleA: [...point.handleB],
      handleB: [...point.handleA],
    })),
  }));

  return { ...source, contours };
}
