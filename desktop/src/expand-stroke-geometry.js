import { EndType, JoinType } from 'clipper2-ts';
import { NectClipperOffset } from './clipper-adapter.js';

const GRID = 1e4;
const GRID_SIZE = 1 / GRID;
const INTEGER_LIMIT = 2 ** 30;
const MAX_CONTOURS = 32;
const MAX_SOURCE_POINTS = 256;
const MAX_OUTPUT_POINTS = 2048;
const MAX_ROUND_WORK = 4096;

const fail = (message) => {
  throw new Error(message);
};
const finiteVector = (value) =>
  Array.isArray(value) && value.length === 2 && value.every(Number.isFinite);
const samePoint = (a, b) => a.x === b.x && a.y === b.y;

function validateStroke(stroke) {
  if (!stroke || typeof stroke !== 'object' || Array.isArray(stroke))
    fail('Expand Stroke needs a connected Stroke node');
  const { width, cap = 'butt', join = 'miter', miterLimit = 4, dash = [] } = stroke;
  if (!Number.isFinite(width) || width < 0 || width > 100000)
    fail('Expand Stroke width must be finite and between 0 and 100000 px');
  if (!['butt', 'square', 'round'].includes(cap)) fail('Expand Stroke has an invalid cap');
  if (!['miter', 'bevel', 'round'].includes(join)) fail('Expand Stroke has an invalid join');
  if (!Number.isFinite(miterLimit) || miterLimit < 1 || miterLimit > 100)
    fail('Expand Stroke miter limit must be between 1 and 100');
  if (
    !Array.isArray(dash) ||
    dash.length > 32 ||
    dash.some((length) => !Number.isFinite(length) || length < 0 || length > 100000)
  )
    fail('Expand Stroke has an invalid dash pattern');
  if (dash.some((length) => length !== 0))
    fail('Expand Stroke supports solid strokes only; remove the dash pattern first');
  if (stroke.dashOffset !== undefined && !Number.isFinite(stroke.dashOffset))
    fail('Expand Stroke has an invalid dash offset');
  return { width, cap, join, miterLimit };
}

function validateSource(source) {
  if (!source || typeof source !== 'object' || !Array.isArray(source.contours))
    fail('Expand Stroke needs a Path input');
  if (source.contours.length > MAX_CONTOURS)
    fail(`Expand Stroke exceeds the ${MAX_CONTOURS}-contour input budget`);

  let pointCount = 0;
  const groups = new Map();
  let worldMagnitude = 0;
  for (const contour of source.contours) {
    if (
      !contour ||
      typeof contour !== 'object' ||
      typeof contour.id !== 'string' ||
      !contour.id ||
      typeof contour.closed !== 'boolean' ||
      !Array.isArray(contour.points) ||
      (contour.group !== undefined && typeof contour.group !== 'string')
    )
      fail('Expand Stroke needs valid contours with stable IDs and point arrays');
    pointCount += contour.points.length;
    if (pointCount > MAX_SOURCE_POINTS)
      fail(`Expand Stroke exceeds the ${MAX_SOURCE_POINTS}-point input budget`);
    const groupId = contour.group ?? '';
    if (!groups.has(groupId)) groups.set(groupId, []);
    const prepared = [];
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
        fail('Expand Stroke needs stable point IDs and finite positions and handles');
      if (
        point.handleA[0] !== 0 ||
        point.handleA[1] !== 0 ||
        point.handleB[0] !== 0 ||
        point.handleB[1] !== 0
      )
        fail('Expand Stroke supports straight-edge paths only. Connect a Resample Path first.');
      worldMagnitude = Math.max(
        worldMagnitude,
        Math.abs(point.position[0]),
        Math.abs(point.position[1]),
      );
      prepared.push({ x: point.position[0], y: point.position[1] });
    }
    if (prepared.length === 1 || (contour.closed && prepared.length === 2))
      fail(
        contour.closed
          ? 'Expand Stroke needs at least three points in each closed contour'
          : 'Expand Stroke needs at least two points in each open contour',
      );
    groups.get(groupId).push({ closed: contour.closed, points: prepared });
  }

  // The local-origin transform keeps Clipper's integer coordinates small. At
  // very large world origins, IEEE-754 restoration would otherwise erase the
  // fixed grid even though the translated integer coordinates are safe.
  if (4 * Number.EPSILON * Math.max(1, worldMagnitude) >= GRID_SIZE)
    fail('Expand Stroke world coordinates are too large for the 0.0001 px output grid');

  return [...groups.values()].map((contours) =>
    contours.filter((contour) => contour.points.length),
  );
}

function localOrigin(contours) {
  for (const contour of contours) if (contour.points.length) return contour.points[0];
  return { x: 0, y: 0 };
}

function pointToGrid(point, origin) {
  const x = Math.round((point.x - origin.x) * GRID);
  const y = Math.round((point.y - origin.y) * GRID);
  if (!Number.isSafeInteger(x) || !Number.isSafeInteger(y))
    fail('Expand Stroke exceeds the safe local integer coordinate range');
  return { x, y };
}

function lineUnit(from, to) {
  const dx = to.x - from.x;
  const dy = to.y - from.y;
  const length = Math.hypot(dx, dy);
  if (!Number.isFinite(length) || length === 0)
    fail('Expand Stroke rejects zero-length source edges');
  return { x: dx / length, y: dy / length };
}

function dropDuplicateGridPoints(points, closed) {
  const result = [];
  for (const point of points)
    if (!result.length || !samePoint(point, result.at(-1))) result.push(point);
  if (closed && result.length > 1 && samePoint(result[0], result.at(-1))) result.pop();
  return result;
}

function roundArcEstimate(contours, cap, join, radius) {
  const needsRound = cap === 'round' || join === 'round';
  if (!needsRound) return 0;
  const arcTolerance = Math.min(0.1, radius / 4) * GRID;
  const radiusGrid = radius * GRID;
  const stepAngle = Math.acos(1 - arcTolerance / radiusGrid);
  if (!Number.isFinite(stepAngle) || stepAngle <= 0)
    fail('Expand Stroke cannot estimate round join work at this width');
  const halfCircleSteps = Math.ceil(Math.PI / stepAngle) + 2;
  let arcCount = 0;
  for (const contour of contours) {
    if (join === 'round')
      arcCount += contour.closed ? contour.points.length : Math.max(0, contour.points.length - 2);
    if (!contour.closed && cap === 'round') arcCount += 2;
  }
  const estimate = arcCount * halfCircleSteps;
  if (!Number.isFinite(estimate) || estimate > MAX_ROUND_WORK)
    fail(`Expand Stroke round joins exceed the ${MAX_ROUND_WORK}-vertex work budget`);
  return estimate;
}

function prepareGroup(contours, { width, cap, join, miterLimit }) {
  const origin = localOrigin(contours);
  const radius = width / 2;
  const reach = radius * (join === 'miter' ? miterLimit : 1) + (cap === 'square' ? radius : 0);
  let maxLocal = 0;
  for (const contour of contours)
    for (const point of contour.points) {
      maxLocal = Math.max(maxLocal, Math.abs(point.x - origin.x), Math.abs(point.y - origin.y));
    }
  if ((maxLocal + reach + GRID_SIZE) * GRID >= INTEGER_LIMIT)
    fail('Expand Stroke local geometry and width/miter reach exceed the safe integer envelope');

  const paths = contours.map((contour) => {
    const points = contour.points.map((point) => ({
      x: point.x - origin.x,
      y: point.y - origin.y,
    }));
    if (!contour.closed && cap === 'square') {
      const firstUnit = lineUnit(points[0], points[1]);
      const lastUnit = lineUnit(points.at(-2), points.at(-1));
      points[0] = {
        x: points[0].x - firstUnit.x * radius,
        y: points[0].y - firstUnit.y * radius,
      };
      points[points.length - 1] = {
        x: points.at(-1).x + lastUnit.x * radius,
        y: points.at(-1).y + lastUnit.y * radius,
      };
    }
    let gridPoints = dropDuplicateGridPoints(
      points.map((point) => pointToGrid(point, { x: 0, y: 0 })),
      contour.closed,
    );
    const minimum = contour.closed ? 3 : 2;
    if (gridPoints.length < minimum)
      fail('Expand Stroke source edges collapse at the 0.0001 px geometry grid');
    // Clipper strips adjacent duplicate points as well, but rejecting a collapsed
    // edge here avoids relying on a changed corner after grid quantization.
    for (let i = 0; i < gridPoints.length - (contour.closed ? 0 : 1); i++) {
      const next = gridPoints[(i + 1) % gridPoints.length];
      if (samePoint(gridPoints[i], next))
        fail('Expand Stroke source edges collapse at the 0.0001 px geometry grid');
    }
    return { closed: contour.closed, points: gridPoints };
  });
  return { origin, paths };
}

/**
 * Convert a solid Stroke result to unpainted filled geometry. This v1 accepts
 * only straight-edge contours; curve handles must first be flattened with
 * Resample Path. It uses a local origin, a 0.0001 px integer grid, and Clipper's
 * polygonal round joins/caps with a configured arc tolerance no greater than
 * 0.1 px. Grid rounding adds up to 0.00005 px per coordinate at each input or
 * output. Integer cleanup can perturb final intersections; this is not a claim
 * of exact SVG stroke equivalence.
 */
export function expandStrokeGeometry(source, options = {}) {
  if (!options || typeof options !== 'object' || Array.isArray(options))
    fail('Expand Stroke needs valid options');
  const { prefix = 'expandStroke' } = options;
  if (typeof prefix !== 'string' || !prefix || prefix.length > 256)
    fail('Expand Stroke needs a nonempty output ID prefix of at most 256 characters');
  const stroke = validateStroke(source?.stroke);
  const groups = validateSource(source);
  if (stroke.width === 0 || groups.every((group) => group.length === 0)) return { contours: [] };
  if (stroke.width * GRID < 2)
    fail('Nonzero Expand Stroke width must be at least 0.0002 px at the 0.0001 px geometry grid');

  const roundWork = groups.reduce(
    (sum, contours) => sum + roundArcEstimate(contours, stroke.cap, stroke.join, stroke.width / 2),
    0,
  );
  if (roundWork > MAX_ROUND_WORK)
    fail(`Expand Stroke round joins exceed the ${MAX_ROUND_WORK}-vertex work budget`);

  const joinTypes = { miter: JoinType.Miter, bevel: JoinType.Bevel, round: JoinType.Round };
  const endTypes = { butt: EndType.Butt, square: EndType.Butt, round: EndType.Round };
  const preparedGroups = groups.map((group) => (group.length ? prepareGroup(group, stroke) : null));
  const contours = [];
  let outputPointCount = 0;
  for (const [groupIndex, prepared] of preparedGroups.entries()) {
    if (!prepared) continue;
    const { origin, paths } = prepared;
    const engine = new NectClipperOffset(stroke.miterLimit, Math.min(0.1, stroke.width / 8) * GRID);
    const openPaths = paths.filter((path) => !path.closed).map((path) => path.points);
    const closedPaths = paths.filter((path) => path.closed).map((path) => path.points);
    // Joined deliberately treats a closed source contour as a centered stroke,
    // producing outer and inner boundaries. Polygon would expand only outward.
    engine.addPaths(openPaths, joinTypes[stroke.join], endTypes[stroke.cap]);
    engine.addPaths(closedPaths, joinTypes[stroke.join], EndType.Joined);
    const solution = [];
    try {
      engine.execute((stroke.width / 2) * GRID, solution);
    } catch {
      fail('Expand Stroke offset kernel could not produce a result');
    }
    for (const ring of solution) {
      outputPointCount += ring.length;
      if (outputPointCount > MAX_OUTPUT_POINTS)
        fail(`Expand Stroke output exceeds the ${MAX_OUTPUT_POINTS}-point geometry budget`);
      if (ring.length < 3) fail('Expand Stroke offset kernel produced a degenerate contour');
      const contourIndex = contours.length;
      contours.push({
        id: `${prefix}:g${groupIndex}:c${contourIndex}`,
        group: `${prefix}:g${groupIndex}`,
        closed: true,
        points: ring.map((point, pointIndex) => {
          if (
            !Number.isSafeInteger(point.x) ||
            !Number.isSafeInteger(point.y) ||
            Math.max(Math.abs(point.x), Math.abs(point.y)) >= INTEGER_LIMIT
          )
            fail('Expand Stroke output exceeds the safe integer coordinate envelope');
          const position = [point.x / GRID + origin.x, point.y / GRID + origin.y];
          if (!position.every(Number.isFinite))
            fail('Expand Stroke output contains non-finite coordinates');
          return {
            id: `${prefix}:g${groupIndex}:c${contourIndex}:p${pointIndex}`,
            position,
            handleA: [0, 0],
            handleB: [0, 0],
          };
        }),
      });
    }
  }
  return { contours };
}
