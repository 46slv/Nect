const MAX_CONTOURS = 10000;
const MAX_POINTS = 100000;

const finiteVector = (value) =>
  Array.isArray(value) && value.length === 2 && value.every(Number.isFinite);

/** Validate path data before a closure edit or Boolean operation. */
export function validatePathGeometry(source, label = 'Set Closure') {
  if (!source || typeof source !== 'object' || !Array.isArray(source.contours))
    throw Error(`${label} needs a Path with contours`);
  if (source.contours.length > MAX_CONTOURS) throw Error(`${label} exceeds geometry budget`);

  let pointCount = 0;
  const contourIds = new Set(),
    pointIds = new Set();
  for (const contour of source.contours) {
    if (
      !contour ||
      typeof contour !== 'object' ||
      typeof contour.id !== 'string' ||
      !contour.id ||
      contourIds.has(contour.id) ||
      typeof contour.closed !== 'boolean' ||
      !Array.isArray(contour.points) ||
      (contour.group !== undefined && typeof contour.group !== 'string')
    )
      throw Error(`${label} needs valid contours and stable IDs`);
    contourIds.add(contour.id);
    pointCount += contour.points.length;
    if (pointCount > MAX_POINTS) throw Error(`${label} exceeds geometry budget`);

    for (const point of contour.points) {
      if (
        !point ||
        typeof point !== 'object' ||
        typeof point.id !== 'string' ||
        !point.id ||
        pointIds.has(point.id) ||
        !finiteVector(point.position) ||
        !finiteVector(point.handleA) ||
        !finiteVector(point.handleB)
      )
        throw Error(`${label} needs stable point IDs and finite positions and handles`);
      pointIds.add(point.id);
      for (let axis = 0; axis < 2; axis++)
        if (
          !Number.isFinite(point.position[axis] + point.handleA[axis]) ||
          !Number.isFinite(point.position[axis] + point.handleB[axis])
        )
          throw Error(`${label} exceeds finite-coordinate arithmetic`);
    }
  }
  return { contourCount: source.contours.length, pointCount };
}

/** Change only each contour's closure flag and derived identities. */
export function setClosureGeometry(source, options = {}) {
  if (!options || typeof options !== 'object' || Array.isArray(options))
    throw Error('Set Closure needs valid options');
  const { closed = true, prefix = 'set-closure' } = options;
  if (typeof closed !== 'boolean') throw Error('Set Closure expects a Boolean Closed value');
  if (typeof prefix !== 'string' || !prefix) throw Error('Set Closure needs a nonempty ID prefix');
  validatePathGeometry(source);

  const copy = structuredClone(source);
  copy.contours = copy.contours.map((contour, contourIndex) => {
    const id = `${prefix}:c${contourIndex}`;
    return {
      ...contour,
      id,
      closed,
      points: contour.points.map((point, pointIndex) => ({
        ...point,
        id: `${id}:p${pointIndex}`,
      })),
    };
  });
  return copy;
}
