import { hasCurves, roundMixedContours } from './corner-round-cubic.js';
const MAX_CONTOURS = 10000;
const MAX_POINTS = 100000;
const ANGLE_EPSILON = 1e-12;

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
  if (!source || !Array.isArray(source.contours)) throw Error('Connect a Path input');
  if (source.contours.length > MAX_CONTOURS) throw Error('Round Corners exceeds geometry budget');
  let pointCount = 0;
  for (const contour of source.contours) {
    if (!contour || typeof contour !== 'object' || typeof contour.closed !== 'boolean')
      throw Error('Round Corners needs valid contours');
    if (!Array.isArray(contour.points)) throw Error('Round Corners needs contour points');
    pointCount += contour.points.length;
    if (pointCount > MAX_POINTS) throw Error('Round Corners exceeds geometry budget');
    for (const point of contour.points) {
      if (
        !point ||
        typeof point !== 'object' ||
        !finiteVector(point.position) ||
        !finiteVector(point.handleA) ||
        !finiteVector(point.handleB)
      )
        throw Error('Round Corners needs finite point positions and handles');
    }
  }
}

function unitEdge(from, to, contourIndex) {
  const dx = to[0] - from[0];
  const dy = to[1] - from[1];
  const length = Math.hypot(dx, dy);
  if (!Number.isFinite(length))
    throw Error(
      `Round Corners contour ${contourIndex} has an edge outside the finite coordinate range`,
    );
  if (length === 0) throw Error(`Round Corners contour ${contourIndex} has a zero-length edge`);
  return { direction: [dx / length, dy / length], length };
}

function cornerArc(vertex, incoming, outgoing, radius, contourIndex) {
  const cross =
    incoming.direction[0] * outgoing.direction[1] - incoming.direction[1] * outgoing.direction[0];
  const dot =
    incoming.direction[0] * outgoing.direction[0] + incoming.direction[1] * outgoing.direction[1];
  const turn = Math.atan2(cross, dot);

  // A straight-through point needs no extra geometry. A 180-degree reversal
  // folds the contour back over itself and has no well-defined fillet.
  if (Math.abs(turn) <= ANGLE_EPSILON) {
    if (dot < 0) throw Error(`Round Corners contour ${contourIndex} has a 180-degree reversal`);
    return null;
  }
  if (Math.PI - Math.abs(turn) <= ANGLE_EPSILON)
    throw Error(`Round Corners contour ${contourIndex} has a 180-degree reversal`);

  const tangentFactor = Math.tan(Math.abs(turn) / 2);
  if (!Number.isFinite(tangentFactor) || tangentFactor <= 0)
    throw Error(`Round Corners contour ${contourIndex} has an unsupported corner`);
  const desired = radius * tangentFactor;
  const tangentDistance = Math.min(
    Number.isFinite(desired) ? desired : Infinity,
    incoming.length / 2,
    outgoing.length / 2,
  );
  if (!(tangentDistance > 0)) return null;

  const effectiveRadius = tangentDistance / tangentFactor;
  const sign = Math.sign(turn);
  const direction = incoming.direction;
  const tangentIn = [
    vertex[0] - direction[0] * tangentDistance,
    vertex[1] - direction[1] * tangentDistance,
  ];
  const normal = sign > 0 ? [-direction[1], direction[0]] : [direction[1], -direction[0]];
  const center = [
    tangentIn[0] + normal[0] * effectiveRadius,
    tangentIn[1] + normal[1] * effectiveRadius,
  ];
  const tangentOut = [
    vertex[0] + outgoing.direction[0] * tangentDistance,
    vertex[1] + outgoing.direction[1] * tangentDistance,
  ];
  if (![effectiveRadius, ...tangentIn, ...center, ...tangentOut].every(Number.isFinite))
    throw Error(`Round Corners contour ${contourIndex} exceeds finite coordinates`);

  const startAngle = Math.atan2(tangentIn[1] - center[1], tangentIn[0] - center[0]);
  const partCount = Math.max(1, Math.ceil(Math.abs(turn) / (Math.PI / 2)));
  const points = [{ position: tangentIn, handleA: [0, 0], handleB: [0, 0] }];
  for (let part = 0; part < partCount; part++) {
    const partTurn = turn / partCount;
    const a0 = startAngle + partTurn * part;
    const a1 = startAngle + partTurn * (part + 1);
    const p0 = points[points.length - 1].position;
    const p3 =
      part === partCount - 1
        ? tangentOut
        : [center[0] + effectiveRadius * Math.cos(a1), center[1] + effectiveRadius * Math.sin(a1)];
    const k = (4 / 3) * Math.tan(Math.abs(partTurn) / 4);
    const startTangent = [-Math.sin(a0) * sign, Math.cos(a0) * sign];
    const endTangent = [-Math.sin(a1) * sign, Math.cos(a1) * sign];
    const control1 = [
      p0[0] + startTangent[0] * k * effectiveRadius,
      p0[1] + startTangent[1] * k * effectiveRadius,
    ];
    const control2 = [
      p3[0] - endTangent[0] * k * effectiveRadius,
      p3[1] - endTangent[1] * k * effectiveRadius,
    ];
    if (![...p3, ...control1, ...control2].every(Number.isFinite))
      throw Error(`Round Corners contour ${contourIndex} exceeds finite coordinates`);
    points[points.length - 1].handleB = [control1[0] - p0[0], control1[1] - p0[1]];
    points.push({
      position: p3,
      handleA: [control2[0] - p3[0], control2[1] - p3[1]],
      handleB: [0, 0],
    });
  }
  return points;
}

const zeroHandle = (handle) => handle[0] === 0 && handle[1] === 0;

// A segment belongs to both endpoints: a zero handle at this vertex alone
// does not make its neighbor's cubic segment a line.
const straightSegment = (from, to) => zeroHandle(from.handleB) && zeroHandle(to.handleA);

function roundContour(contour, contourIndex, radius, prefix, addPoint) {
  const points = contour.points;
  const count = points.length;
  const closed = contour.closed;
  if (count === 0) return { ...contour, id: `${prefix}:c${contourIndex}`, points: [] };
  if (closed && count < 3)
    return {
      ...contour,
      id: `${prefix}:c${contourIndex}`,
      points: points.map((point, index) => {
        addPoint();
        return {
          ...point,
          id: `${prefix}:c${contourIndex}:p${index}`,
          position: [...point.position],
          handleA: [...point.handleA],
          handleB: [...point.handleB],
        };
      }),
    };

  const corners = new Array(count).fill(null);
  const first = closed ? 0 : 1;
  const end = closed ? count : count - 1;
  for (let i = first; i < end; i++) {
    const previous = (i - 1 + count) % count;
    const next = (i + 1) % count;
    if (!straightSegment(points[previous], points[i]) || !straightSegment(points[i], points[next]))
      continue;
    const incoming = unitEdge(points[previous].position, points[i].position, contourIndex);
    const outgoing = unitEdge(points[i].position, points[next].position, contourIndex);
    corners[i] = cornerArc(points[i].position, incoming, outgoing, radius, contourIndex);
  }

  const output = [];
  const push = (point) => {
    addPoint();
    const { id: sourceId, ...attributes } = point;
    output.push({
      id: `${prefix}:c${contourIndex}:p${output.length}`,
      ...attributes,
      position: [...point.position],
      handleA: [...point.handleA],
      handleB: [...point.handleB],
    });
  };
  if (closed) {
    for (let i = 0; i < count; i++) {
      const arc = corners[i];
      if (arc) for (const point of arc) push(point);
      else push(points[i]);
    }
  } else {
    push(points[0]);
    for (let i = 1; i < count - 1; i++) {
      const arc = corners[i];
      if (arc) for (const point of arc) push(point);
      else push(points[i]);
    }
    if (count > 1) push(points[count - 1]);
  }
  return {
    ...contour,
    id: `${prefix}:c${contourIndex}`,
    points: output,
  };
}

/**
 * Preserve the historical straight-only path. Mixed contours use conservative
 * G1 blends with exact retained cubic restrictions and bounded diagnostics.
 * A zero radius is a geometry-preserving clone/no-op.
 */
export function roundCorners(source, { radius, prefix = 'round-corners' } = {}) {
  if (!Number.isFinite(radius) || radius < 0)
    throw Error('Round Corners radius must be finite and nonnegative');
  validateSource(source);
  if (radius === 0) return cloneGeometry(source);
  if (typeof prefix !== 'string' || !prefix)
    throw Error('Round Corners needs a nonempty ID prefix');
  let pointCount = 0;
  const addPoint = () => {
    pointCount++;
    if (pointCount > MAX_POINTS) throw Error('Round Corners exceeds geometry budget');
  };
  if (source.contours.some(hasCurves))
    return roundMixedContours(source, {
      radius,
      prefix,
      addPoint,
      legacy: (contour, index) => roundContour(contour, index, radius, prefix, addPoint),
    });
  const contours = source.contours.map((contour, index) =>
    roundContour(contour, index, radius, prefix, () => {
      pointCount++;
      if (pointCount > MAX_POINTS) throw Error('Round Corners exceeds geometry budget');
    }),
  );
  return { contours };
}
