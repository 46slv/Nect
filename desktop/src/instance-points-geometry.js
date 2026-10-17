const MAX_CONTOURS = 10_000;
const MAX_POINTS = 100_000;
const MAX_INSTANCES = 256;
const MAX_GROUPS = 256;
const MAX_POINT_INSTANCE_WORK = 2_000_000;
const ROOT_GEOMETRY_KEYS = new Set(['contours']);
const CONTOUR_GEOMETRY_KEYS = new Set(['id', 'group', 'points']);
const POINT_GEOMETRY_KEYS = new Set(['id', 'position', 'handleA', 'handleB']);

const finiteVector = (value) =>
  Array.isArray(value) &&
  value.length === 2 &&
  Number.isFinite(value[0]) &&
  Number.isFinite(value[1]);

function validatePath(path, label) {
  if (!path || typeof path !== 'object' || !Array.isArray(path.contours))
    throw Error(`${label} needs a Path with contours`);
  if (path.contours.length > MAX_CONTOURS) throw Error(`${label} exceeds geometry budget`);

  let pointCount = 0;
  const contourIds = new Set();
  const pointIds = new Set();
  for (const contour of path.contours) {
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
      for (let axis = 0; axis < 2; axis++) {
        if (
          !Number.isFinite(point.position[axis] + point.handleA[axis]) ||
          !Number.isFinite(point.position[axis] + point.handleB[axis])
        )
          throw Error(`${label} exceeds finite-coordinate arithmetic`);
      }
    }
  }
  return { pointCount };
}

function copyMetadata(object, excludedKeys) {
  const copy = {};
  for (const [key, value] of Object.entries(object)) {
    if (!excludedKeys.has(key)) copy[key] = structuredClone(value);
  }
  return copy;
}

function emptyPath(source) {
  return { ...copyMetadata(source, ROOT_GEOMETRY_KEYS), contours: [] };
}

/**
 * Materialize a translation-only copy of source at every target anchor. The
 * caller supplies the exact whole-source geometric bounds center as pivot.
 */
export function instancePointsGeometry(source, targets, options = {}) {
  if (!options || typeof options !== 'object' || Array.isArray(options))
    throw Error('Instance on Points needs valid options');
  const { pivot, prefix = 'instances' } = options;
  if (typeof prefix !== 'string' || !prefix)
    throw Error('Instance on Points needs a nonempty ID prefix');

  const sourceInfo = validatePath(source, 'Instance on Points source');
  const targetInfo = validatePath(targets, 'Instance on Points targets');

  const anchors = [];
  for (const contour of targets.contours) {
    for (const point of contour.points) anchors.push(point.position);
  }
  if (anchors.length > MAX_INSTANCES) throw Error('Instance on Points exceeds instance budget');

  // No geometry can be emitted without anchors in both inputs. In particular,
  // an irrelevant pivot is not required for an empty source or target.
  if (sourceInfo.pointCount === 0 || targetInfo.pointCount === 0) return emptyPath(source);
  if (!finiteVector(pivot)) throw Error('Instance on Points needs a finite Vector2 pivot');

  const sourceGroups = new Map();
  for (const contour of source.contours) {
    const key = contour.group ?? '';
    if (!sourceGroups.has(key)) sourceGroups.set(key, sourceGroups.size);
  }

  const outputContourCount = source.contours.length * anchors.length;
  const outputPointCount = sourceInfo.pointCount * anchors.length;
  const outputGroupCount = sourceGroups.size * anchors.length;
  const pointInstanceWork = sourceInfo.pointCount * anchors.length;
  if (
    outputContourCount > MAX_CONTOURS ||
    outputPointCount > MAX_POINTS ||
    outputGroupCount > MAX_GROUPS ||
    pointInstanceWork > MAX_POINT_INSTANCE_WORK
  )
    throw Error('Instance on Points exceeds geometry budget');

  // Preflight all arithmetic before allocating any output geometry. Handles are
  // relative vectors, so only their translated absolute positions are checked.
  const deltas = anchors.map((position) => {
    const delta = [position[0] - pivot[0], position[1] - pivot[1]];
    if (!delta.every(Number.isFinite)) throw Error('Instance on Points exceeds finite coordinates');
    return delta;
  });
  for (const delta of deltas) {
    for (const contour of source.contours) {
      for (const point of contour.points) {
        const x = point.position[0] + delta[0];
        const y = point.position[1] + delta[1];
        if (
          !Number.isFinite(x) ||
          !Number.isFinite(y) ||
          !Number.isFinite(x + point.handleA[0]) ||
          !Number.isFinite(y + point.handleA[1]) ||
          !Number.isFinite(x + point.handleB[0]) ||
          !Number.isFinite(y + point.handleB[1])
        )
          throw Error('Instance on Points exceeds finite coordinates');
      }
    }
  }

  const contours = [];
  for (let instanceIndex = 0; instanceIndex < deltas.length; instanceIndex++) {
    const delta = deltas[instanceIndex];
    for (let contourIndex = 0; contourIndex < source.contours.length; contourIndex++) {
      const contour = source.contours[contourIndex];
      const groupIndex = sourceGroups.get(contour.group ?? '');
      const contourId = `${prefix}:i${instanceIndex}:c${contourIndex}`;
      contours.push({
        ...copyMetadata(contour, CONTOUR_GEOMETRY_KEYS),
        id: contourId,
        group: `${prefix}:i${instanceIndex}:g${groupIndex}`,
        closed: contour.closed,
        points: contour.points.map((point, pointIndex) => ({
          ...copyMetadata(point, POINT_GEOMETRY_KEYS),
          id: `${contourId}:p${pointIndex}`,
          position: [point.position[0] + delta[0], point.position[1] + delta[1]],
          handleA: [...point.handleA],
          handleB: [...point.handleB],
        })),
      });
    }
  }

  return { ...copyMetadata(source, ROOT_GEOMETRY_KEYS), contours };
}
