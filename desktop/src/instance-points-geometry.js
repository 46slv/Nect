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

export function validateInstancePath(path, label) {
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

function subtractVector(left, right) {
  const result = [left[0] - right[0], left[1] - right[1]];
  if (!result.every(Number.isFinite)) throw Error('Instance on Points exceeds finite coordinates');
  return result;
}

function isNonzeroVector(vector) {
  return vector[0] !== 0 || vector[1] !== 0;
}

function tangentForPoint(contour, pointIndex) {
  const point = contour.points[pointIndex];
  if (isNonzeroVector(point.handleB)) return point.handleB;

  const reverseIncoming = [-point.handleA[0], -point.handleA[1]];
  if (isNonzeroVector(reverseIncoming)) return reverseIncoming;

  const points = contour.points;
  const count = points.length;
  const closed = contour.closed;
  const current = point.position;
  const nextIndex = pointIndex + 1 < count ? pointIndex + 1 : closed ? 0 : -1;
  const previousIndex = pointIndex > 0 ? pointIndex - 1 : closed ? count - 1 : -1;
  let tangent = [0, 0];

  if (count > 1) {
    if (closed) {
      tangent = subtractVector(points[nextIndex].position, points[previousIndex].position);
    } else if (pointIndex > 0 && pointIndex < count - 1) {
      tangent = subtractVector(points[nextIndex].position, points[previousIndex].position);
    } else if (pointIndex === 0) {
      tangent = subtractVector(points[nextIndex].position, current);
    } else {
      tangent = subtractVector(current, points[previousIndex].position);
    }
    if (isNonzeroVector(tangent)) return tangent;

    // A centered chord can cancel at a cusp or where anchors coincide. Prefer
    // the forward edge, then the incoming edge in its path-forward direction.
    if (nextIndex !== -1 && nextIndex !== pointIndex) {
      tangent = subtractVector(points[nextIndex].position, current);
      if (isNonzeroVector(tangent)) return tangent;
    }
    if (previousIndex !== -1 && previousIndex !== pointIndex) {
      tangent = subtractVector(current, points[previousIndex].position);
      if (isNonzeroVector(tangent)) return tangent;
    }
  }

  return [1, 0];
}

function rotateVector(vector, cosine, sine) {
  const result = [vector[0] * cosine - vector[1] * sine, vector[0] * sine + vector[1] * cosine];
  if (!result.every(Number.isFinite)) throw Error('Instance on Points exceeds finite coordinates');
  return result;
}

/**
 * Materialize source at every target anchor using the supplied whole-source
 * geometric bounds center as pivot. By default this remains translation-only;
 * alignTangent rotates each copy so its +X axis follows the target path.
 */
export function instancePointsGeometry(source, targets, options = {}) {
  if (!options || typeof options !== 'object' || Array.isArray(options))
    throw Error('Instance on Points needs valid options');
  const { pivot, prefix = 'instances', alignTangent = false, transforms } = options;
  if (typeof prefix !== 'string' || !prefix)
    throw Error('Instance on Points needs a nonempty ID prefix');
  if (typeof alignTangent !== 'boolean')
    throw Error('Instance on Points needs a boolean alignTangent option');

  const sourceInfo = validateInstancePath(source, 'Instance on Points source');
  const targetInfo = validateInstancePath(targets, 'Instance on Points targets');

  const anchors = [];
  const anchorContexts = [];
  for (const contour of targets.contours) {
    for (let pointIndex = 0; pointIndex < contour.points.length; pointIndex++) {
      anchors.push(contour.points[pointIndex].position);
      anchorContexts.push({ contour, pointIndex });
    }
  }
  if (anchors.length > MAX_INSTANCES) throw Error('Instance on Points exceeds instance budget');

  if (
    transforms !== undefined &&
    (!Array.isArray(transforms) ||
      transforms.length !== anchors.length ||
      transforms.some(
        (t) => !t || !Number.isFinite(t.scale) || t.scale < 0 || !Number.isFinite(t.rotation),
      ))
  )
    throw Error('Instance on Points needs finite per-instance scale and rotation');
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

  let deltas;
  let alignedPoints;
  if (alignTangent || transforms) {
    // Preflight transformed anchors and handles before allocating output. The
    // source pivot maps to each target point; handles remain relative vectors.
    alignedPoints = anchorContexts.map(({ contour, pointIndex }, instanceIndex) => {
      const tangent = tangentForPoint(contour, pointIndex);
      const angle = transforms
        ? (transforms[instanceIndex].rotation * Math.PI) / 180
        : Math.atan2(tangent[1], tangent[0]);
      const scale = transforms ? transforms[instanceIndex].scale : 1;
      const cosine = Math.cos(angle) * scale;
      const sine = Math.sin(angle) * scale;
      const anchor = anchors[instanceIndex];
      return source.contours.map((sourceContour) =>
        sourceContour.points.map((point) => {
          const relativePosition = subtractVector(point.position, pivot);
          const rotatedPosition = rotateVector(relativePosition, cosine, sine);
          const position = [rotatedPosition[0] + anchor[0], rotatedPosition[1] + anchor[1]];
          const handleA = rotateVector(point.handleA, cosine, sine);
          const handleB = rotateVector(point.handleB, cosine, sine);
          if (
            !position.every(Number.isFinite) ||
            !Number.isFinite(position[0] + handleA[0]) ||
            !Number.isFinite(position[1] + handleA[1]) ||
            !Number.isFinite(position[0] + handleB[0]) ||
            !Number.isFinite(position[1] + handleB[1])
          )
            throw Error('Instance on Points exceeds finite coordinates');
          return { position, handleA, handleB };
        }),
      );
    });
  } else {
    // Preserve the original translation-only arithmetic and emitted geometry.
    deltas = anchors.map((position) => {
      const delta = [position[0] - pivot[0], position[1] - pivot[1]];
      if (!delta.every(Number.isFinite))
        throw Error('Instance on Points exceeds finite coordinates');
      return delta;
    });
    // Preflight all arithmetic before allocating any output geometry. Handles
    // are relative vectors, so only their translated absolute positions matter.
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
  }

  const contours = [];
  for (let instanceIndex = 0; instanceIndex < anchors.length; instanceIndex++) {
    const delta = deltas?.[instanceIndex];
    for (let contourIndex = 0; contourIndex < source.contours.length; contourIndex++) {
      const contour = source.contours[contourIndex];
      const groupIndex = sourceGroups.get(contour.group ?? '');
      const contourId = `${prefix}:i${instanceIndex}:c${contourIndex}`;
      contours.push({
        ...copyMetadata(contour, CONTOUR_GEOMETRY_KEYS),
        id: contourId,
        group: `${prefix}:i${instanceIndex}:g${groupIndex}`,
        closed: contour.closed,
        points: contour.points.map((point, pointIndex) => {
          const transformed = alignedPoints?.[instanceIndex]?.[contourIndex]?.[pointIndex];
          return {
            ...copyMetadata(point, POINT_GEOMETRY_KEYS),
            id: `${contourId}:p${pointIndex}`,
            position: transformed
              ? [...transformed.position]
              : [point.position[0] + delta[0], point.position[1] + delta[1]],
            handleA: transformed ? [...transformed.handleA] : [...point.handleA],
            handleB: transformed ? [...transformed.handleB] : [...point.handleB],
          };
        }),
      });
    }
  }

  return { ...copyMetadata(source, ROOT_GEOMETRY_KEYS), contours };
}
