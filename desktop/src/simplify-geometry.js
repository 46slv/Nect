const MAX_CONTOURS = 10000;
const MAX_POINTS = 100000;
const MAX_DISTANCE_CHECKS = 2000000;
const MAX_TOLERANCE = 100000;

const finiteVector = (value) =>
  Array.isArray(value) && value.length === 2 && value.every(Number.isFinite);

function validateSource(source) {
  if (!source || !Array.isArray(source.contours)) throw Error('Connect a Path input');
  if (source.contours.length > MAX_CONTOURS) throw Error('Simplify exceeds geometry budget');

  let pointCount = 0;
  for (const contour of source.contours) {
    if (
      !contour ||
      typeof contour !== 'object' ||
      typeof contour.id !== 'string' ||
      !contour.id ||
      typeof contour.closed !== 'boolean'
    )
      throw Error('Simplify needs valid contours with stable IDs');
    if (!Array.isArray(contour.points)) throw Error('Simplify needs contour points');
    pointCount += contour.points.length;
    if (pointCount > MAX_POINTS) throw Error('Simplify exceeds geometry budget');
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
        throw Error('Simplify needs stable point IDs and finite positions and handles');
    }
  }
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

function cloneWithKeptIndices(source, keptByContour, prefix) {
  return {
    ...source,
    contours: source.contours.map((contour, contourIndex) => ({
      ...contour,
      id: `${prefix}:c${contourIndex}`,
      points: keptByContour[contourIndex].map((sourceIndex, outputIndex) => {
        const point = contour.points[sourceIndex];
        return {
          ...point,
          id: `${prefix}:c${contourIndex}:p${outputIndex}`,
          position: [...point.position],
          handleA: [...point.handleA],
          handleB: [...point.handleB],
        };
      }),
    })),
  };
}

function assertFiniteIntermediate(value) {
  if (!Number.isFinite(value)) throw Error('Simplify exceeds finite-coordinate arithmetic');
  return value;
}

function checkedDifference(a, b) {
  return assertFiniteIntermediate(a - b);
}

function distanceToSegment(point, start, end) {
  const dx = checkedDifference(end[0], start[0]);
  const dy = checkedDifference(end[1], start[1]);
  const px = checkedDifference(point[0], start[0]);
  const py = checkedDifference(point[1], start[1]);
  const length = assertFiniteIntermediate(Math.hypot(dx, dy));

  if (length === 0) return assertFiniteIntermediate(Math.hypot(px, py));

  const unitX = dx / length;
  const unitY = dy / length;
  const projection = assertFiniteIntermediate(px * unitX + py * unitY);
  if (projection <= 0) return assertFiniteIntermediate(Math.hypot(px, py));
  if (projection >= length) {
    const endX = checkedDifference(point[0], end[0]);
    const endY = checkedDifference(point[1], end[1]);
    return assertFiniteIntermediate(Math.hypot(endX, endY));
  }

  const perpendicular = assertFiniteIntermediate(px * -unitY + py * unitX);
  return Math.abs(perpendicular);
}

function euclideanDistance(a, b) {
  const dx = checkedDifference(a[0], b[0]);
  const dy = checkedDifference(a[1], b[1]);
  return assertFiniteIntermediate(Math.hypot(dx, dy));
}

function nextDistanceCheck(work) {
  work.checks++;
  if (work.checks > MAX_DISTANCE_CHECKS) throw Error('Simplify exceeds distance-work budget');
}

// Iterative Ramer-Douglas-Peucker. Ties keep the earliest source point.
function simplifyOpenIndices(points, indices, tolerance, work) {
  const kept = new Set();
  if (indices.length === 0) return kept;
  kept.add(indices[0]);
  if (indices.length === 1) return kept;
  kept.add(indices[indices.length - 1]);

  const stack = [[0, indices.length - 1]];
  while (stack.length) {
    const [startIndex, endIndex] = stack.pop();
    const startPoint = points[indices[startIndex]].position;
    const endPoint = points[indices[endIndex]].position;
    let farthestIndex = -1;
    let farthestDistance = tolerance;

    for (let index = startIndex + 1; index < endIndex; index++) {
      nextDistanceCheck(work);
      const distance = distanceToSegment(points[indices[index]].position, startPoint, endPoint);
      if (distance > farthestDistance) {
        farthestDistance = distance;
        farthestIndex = index;
      }
    }

    if (farthestIndex !== -1) {
      kept.add(indices[farthestIndex]);
      // Push right first so the left interval is processed next.
      stack.push([farthestIndex, endIndex]);
      stack.push([startIndex, farthestIndex]);
    }
  }

  return kept;
}

function normalizedPositions(points) {
  let scale = 0;
  for (const point of points) {
    scale = Math.max(scale, Math.abs(point.position[0]), Math.abs(point.position[1]));
  }
  if (scale === 0) return points.map(() => [0, 0]);
  return points.map((point) => [point.position[0] / scale, point.position[1] / scale]);
}

function hasThreeDistinctNonCollinear(points) {
  if (points.length < 3) return false;
  const unique = [];
  const seen = new Set();
  for (const point of points) {
    const [x, y] = point.position;
    const key = `${x === 0 ? 0 : x},${y === 0 ? 0 : y}`;
    if (!seen.has(key)) {
      seen.add(key);
      unique.push(point);
    }
  }
  if (unique.length < 3) return false;

  const normalized = normalizedPositions(unique);
  const [ax, ay] = normalized[0];
  const secondIndex = normalized.findIndex((position) =>
    position.some((value, axis) => value !== normalized[0][axis]),
  );
  if (secondIndex < 1) return false;
  const [bx, by] = normalized[secondIndex];
  for (let index = secondIndex + 1; index < normalized.length; index++) {
    const [cx, cy] = normalized[index];
    const cross = assertFiniteIntermediate((bx - ax) * (cy - ay) - (by - ay) * (cx - ax));
    if (cross !== 0) return true;
  }
  return false;
}

function signedAreaSign(points) {
  if (points.length < 3) return 0;
  const normalized = normalizedPositions(points);
  const [originX, originY] = normalized[0];
  let sum = 0;
  let compensation = 0;
  for (let index = 0; index < normalized.length; index++) {
    const current = normalized[index];
    const next = normalized[(index + 1) % normalized.length];
    const term = assertFiniteIntermediate(
      (current[0] - originX) * (next[1] - originY) - (current[1] - originY) * (next[0] - originX),
    );
    const adjusted = term - compensation;
    const nextSum = assertFiniteIntermediate(sum + adjusted);
    compensation = nextSum - sum - adjusted;
    sum = nextSum;
  }
  return Math.sign(sum);
}

function ringIsValid(points) {
  return hasThreeDistinctNonCollinear(points) && signedAreaSign(points) !== 0;
}

function simplifyClosedIndices(points, tolerance, work) {
  const count = points.length;
  const allIndices = Array.from({ length: count }, (_, index) => index);
  if (count < 3 || !ringIsValid(points)) return allIndices;

  const seam = points[0].position;
  let split = 1;
  let farthestDistance = -1;
  for (let index = 1; index < count; index++) {
    nextDistanceCheck(work);
    const distance = euclideanDistance(points[index].position, seam);
    if (distance > farthestDistance) {
      farthestDistance = distance;
      split = index;
    }
  }

  const firstArc = Array.from({ length: split + 1 }, (_, index) => index);
  const secondArc = [
    split,
    ...Array.from({ length: count - split - 1 }, (_, index) => split + index + 1),
    0,
  ];
  const kept = new Set([
    ...simplifyOpenIndices(points, firstArc, tolerance, work),
    ...simplifyOpenIndices(points, secondArc, tolerance, work),
  ]);
  const indices = [...kept].sort((a, b) => a - b);
  const reduced = indices.map((index) => points[index]);

  if (!hasThreeDistinctNonCollinear(reduced) || signedAreaSign(reduced) !== signedAreaSign(points))
    return allIndices;
  return indices;
}

/**
 * Reduce straight-edge paths with bounded Ramer-Douglas-Peucker simplification.
 * Tolerance is measured in document pixels. This is an approximation, not a
 * topology-preserving operation: narrow features, holes, and intersections can
 * change within tolerance, and no Boolean repair is guaranteed.
 */
export function simplifyGeometry(source, options = {}) {
  if (!options || typeof options !== 'object') throw Error('Simplify needs valid options');
  const { tolerance, prefix = 'simplify' } = options;
  if (!Number.isFinite(tolerance) || tolerance < 0 || tolerance > MAX_TOLERANCE)
    throw Error('Simplify tolerance must be finite and between 0 and 100000');
  if (typeof prefix !== 'string' || !prefix) throw Error('Simplify needs a nonempty ID prefix');

  validateSource(source);
  if (tolerance === 0) return cloneGeometry(source);

  for (const contour of source.contours) {
    if (
      contour.points.some(
        (point) =>
          point.handleA.some((value) => value !== 0) || point.handleB.some((value) => value !== 0),
      )
    )
      throw Error('Simplify needs straight-edge contours; use Resample Path first');
  }

  const work = { checks: 0 };
  const keptByContour = source.contours.map((contour) => {
    const indices = contour.closed
      ? simplifyClosedIndices(contour.points, tolerance, work)
      : [
          ...simplifyOpenIndices(
            contour.points,
            Array.from({ length: contour.points.length }, (_, index) => index),
            tolerance,
            work,
          ),
        ].sort((a, b) => a - b);
    return indices;
  });

  return cloneWithKeptIndices(source, keptByContour, prefix);
}
