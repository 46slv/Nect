import { FillRule, intersect } from 'clipper2-ts';
import { flattenCubicCenterline } from './closed-offset.js';

const GRID = 1e4;
const GRID_SIZE = 1 / GRID;
const INTEGER_LIMIT = 2 ** 30;
const FLATTEN_TOLERANCE = 0.1;

export const LUMINANCE_MASK_LIMITS = Object.freeze({
  // Preparation accepts ordinary multi-glyph outlines and owned 10000-point paths.
  // Clipping work and generated/output caps below remain independent hard limits.
  contours: 256,
  sourcePoints: 10000,
  flattenedVertices: 32768,
  subdivisionNodes: 131072,
  subdivisionDepth: 24,
  generatedVertices: 100000,
  pairWork: 2000000,
  outputVertices: 100000,
  outputContours: 10000,
  grid: GRID,
  integerMagnitude: INTEGER_LIMIT,
});

export class LuminanceMaskError extends Error {
  constructor(code, message) {
    super(message);
    this.name = 'LuminanceMaskError';
    this.code = code;
  }
}

const fail = (code, message) => {
  throw new LuminanceMaskError(code, message);
};
const check = (condition, code, message) => {
  if (!condition) fail(code, message);
};
const isRecord = (value) => value !== null && typeof value === 'object' && !Array.isArray(value);
const same = (a, b) => a.x === b.x && a.y === b.y;
const magnitude = (point) => Math.max(Math.abs(point.x), Math.abs(point.y));
const finiteVector = (value) =>
  Array.isArray(value) && value.length === 2 && value.every(Number.isFinite);
const worldPoint = (local, origin) => [local.x + origin.x, local.y + origin.y];

function vector(value, label) {
  check(finiteVector(value), 'LUMINANCE_MASK_GEOMETRY', `${label} must be a finite Vector2`);
  return { x: value[0], y: value[1] };
}

function integerPoint(point, origin, label) {
  const localX = point[0] - origin.x;
  const localY = point[1] - origin.y;
  check(
    Number.isFinite(localX) && Number.isFinite(localY),
    'LUMINANCE_MASK_RANGE',
    `${label} is outside the safe local coordinate range`,
  );
  const x = Math.round(localX * GRID);
  const y = Math.round(localY * GRID);
  check(
    Number.isSafeInteger(x) &&
      Number.isSafeInteger(y) &&
      Math.max(Math.abs(x), Math.abs(y)) < INTEGER_LIMIT,
    'LUMINANCE_MASK_RANGE',
    `${label} exceeds the safe integer coordinate envelope`,
  );
  return { x, y };
}

function checkNoCollapsedEdges(points, label) {
  check(points.length >= 3, 'LUMINANCE_MASK_GEOMETRY', `${label} needs at least three vertices`);
  for (let i = 0; i < points.length; i++) {
    check(
      points[i].x !== points[(i + 1) % points.length].x ||
        points[i].y !== points[(i + 1) % points.length].y,
      'LUMINANCE_MASK_PRECISION',
      `${label} has an edge that collapses on the 0.0001 px grid`,
    );
  }
}

/**
 * Prepare a closed, ungrouped Nect Path as bounded flattened polygons.
 * Polygon coordinates and returned bounds remain in document/world space;
 * `origin` is retained so later Clipper calls can safely use local integers.
 */
export function prepareLuminanceMask(path) {
  check(
    isRecord(path) && Array.isArray(path.contours),
    'LUMINANCE_MASK_GEOMETRY',
    'Luminance Mask needs a Nect Path input',
  );
  check(
    path.contours.length > 0,
    'LUMINANCE_MASK_GEOMETRY',
    'Luminance Mask needs at least one closed contour',
  );
  check(
    path.contours.length <= LUMINANCE_MASK_LIMITS.contours,
    'LUMINANCE_MASK_LIMIT',
    `Luminance Mask exceeds the ${LUMINANCE_MASK_LIMITS.contours}-contour input budget. Reduce source contours.`,
  );
  check(
    path.contours.every((contour) => isRecord(contour)),
    'LUMINANCE_MASK_GEOMETRY',
    'Luminance Mask contours must be valid contour objects',
  );
  check(
    path.contours.every((contour) => contour.group === undefined),
    'LUMINANCE_MASK_UNSUPPORTED',
    'Luminance Mask supports ungrouped Paths only; resolve grouped contours first',
  );

  let sourceCount = 0;
  let origin = null;
  let worldMagnitude = 0;
  const authored = path.contours.map((contour) => {
    check(
      contour.closed === true,
      'LUMINANCE_MASK_OPEN_PATH',
      'Luminance Mask needs closed contours; open Paths cannot define fill area',
    );
    check(
      Array.isArray(contour.points) && contour.points.length >= 3,
      'LUMINANCE_MASK_GEOMETRY',
      'Luminance Mask contours need at least three authored points',
    );
    sourceCount += contour.points.length;
    check(
      sourceCount <= LUMINANCE_MASK_LIMITS.sourcePoints,
      'LUMINANCE_MASK_LIMIT',
      `Luminance Mask exceeds the ${LUMINANCE_MASK_LIMITS.sourcePoints}-point input budget. Simplify the source path.`,
    );
    return contour.points.map((point) => {
      check(isRecord(point), 'LUMINANCE_MASK_GEOMETRY', 'Luminance Mask point is invalid');
      const position = vector(point.position, 'Luminance Mask point position');
      const handleA = vector(point.handleA, 'Luminance Mask incoming handle');
      const handleB = vector(point.handleB, 'Luminance Mask outgoing handle');
      if (!origin) origin = position;
      worldMagnitude = Math.max(worldMagnitude, Math.abs(position.x), Math.abs(position.y));
      const anchor = { x: position.x - origin.x, y: position.y - origin.y };
      const control = (handle, label) => {
        const result = { x: anchor.x + handle.x, y: anchor.y + handle.y };
        check(
          (handle.x === 0 || result.x !== anchor.x) && (handle.y === 0 || result.y !== anchor.y),
          'LUMINANCE_MASK_PRECISION',
          `${label} was lost while forming a cubic control point`,
        );
        return result;
      };
      return {
        anchor,
        incoming: control(handleA, 'Incoming handle'),
        outgoing: control(handleB, 'Outgoing handle'),
      };
    });
  });

  let maxLocal = 0;
  for (const contour of authored)
    for (const point of contour)
      maxLocal = Math.max(
        maxLocal,
        magnitude(point.anchor),
        magnitude(point.incoming),
        magnitude(point.outgoing),
      );
  check(
    (maxLocal + 1) * GRID < INTEGER_LIMIT,
    'LUMINANCE_MASK_RANGE',
    'Luminance Mask geometry exceeds the safe integer coordinate envelope',
  );

  const floatGuard = 256 * Number.EPSILON * Math.max(1, maxLocal, worldMagnitude);
  check(
    floatGuard < FLATTEN_TOLERANCE / 4,
    'LUMINANCE_MASK_PRECISION',
    'World coordinates are too large for reliable 0.1 px curve flattening',
  );

  let flattenedVertices = 0;
  let subdivisionNodes = 0;
  let maximumCurveError = 0;
  const polygons = authored.map((contour) => {
    const points = [];
    for (let i = 0; i < contour.length; i++) {
      const current = contour[i];
      const next = contour[(i + 1) % contour.length];
      const controls = [current.anchor, current.outgoing, next.incoming, next.anchor];
      // Font outlines can contain repeated anchors with zero handles. They
      // contribute no geometry and cannot satisfy a monotone cubic subdivision.
      if (controls.every((point) => same(point, controls[0]))) continue;
      let flattened;
      try {
        flattened = flattenCubicCenterline(controls, {
          tolerance: FLATTEN_TOLERANCE - floatGuard,
          floatGuard,
          maxVertices: LUMINANCE_MASK_LIMITS.flattenedVertices - flattenedVertices,
          maxNodes: LUMINANCE_MASK_LIMITS.subdivisionNodes - subdivisionNodes,
        });
      } catch {
        fail(
          'LUMINANCE_MASK_LIMIT',
          `Luminance Mask exceeds the ${LUMINANCE_MASK_LIMITS.flattenedVertices}-vertex or ${LUMINANCE_MASK_LIMITS.subdivisionNodes}-subdivision curve budget. Simplify the source path.`,
        );
      }
      flattenedVertices += flattened.flattenedVertices;
      subdivisionNodes += flattened.subdivisionNodes;
      check(
        flattenedVertices <= LUMINANCE_MASK_LIMITS.flattenedVertices &&
          subdivisionNodes <= LUMINANCE_MASK_LIMITS.subdivisionNodes,
        'LUMINANCE_MASK_LIMIT',
        'Luminance Mask curve flattening exceeds its work budget',
      );
      for (let leafIndex = 0; leafIndex < flattened.leaves.length; leafIndex++) {
        const { controls: leaf, error } = flattened.leaves[leafIndex];
        if (points.length === 0) points.push(leaf[0]);
        maximumCurveError = Math.max(maximumCurveError, error);
        const isClosingEndpoint =
          i === contour.length - 1 && leafIndex === flattened.leaves.length - 1;
        if (!isClosingEndpoint) points.push(leaf[3]);
      }
    }
    // A redundant closing authored anchor may leave the final endpoint twice.
    if (points.length > 1 && same(points[0], points.at(-1))) points.pop();
    check(
      points.length >= 3,
      'LUMINANCE_MASK_GEOMETRY',
      'Luminance Mask flattening produced a degenerate contour',
    );
    const world = points.map((point) => worldPoint(point, origin));
    const integer = world.map((point) => integerPoint(point, origin, 'Luminance Mask contour'));
    checkNoCollapsedEdges(integer, 'Luminance Mask contour');
    return world;
  });

  let minX = Infinity,
    minY = Infinity,
    maxX = -Infinity,
    maxY = -Infinity;
  for (const polygon of polygons)
    for (const [x, y] of polygon) {
      minX = Math.min(minX, x);
      minY = Math.min(minY, y);
      maxX = Math.max(maxX, x);
      maxY = Math.max(maxY, y);
    }
  const boundsPadding = maximumCurveError + Math.SQRT1_2 * GRID_SIZE + floatGuard;
  const bounds = {
    minX: minX - boundsPadding,
    minY: minY - boundsPadding,
    maxX: maxX + boundsPadding,
    maxY: maxY + boundsPadding,
  };

  return {
    polygons,
    bounds,
    origin: { x: origin.x, y: origin.y },
    flattenedVertices,
    subdivisionNodes,
  };
}

function validateMask(mask) {
  check(
    isRecord(mask) && Array.isArray(mask.polygons) && isRecord(mask.origin),
    'LUMINANCE_MASK_GEOMETRY',
    'Luminance Mask data is invalid',
  );
  check(
    finiteVector([mask.origin.x, mask.origin.y]),
    'LUMINANCE_MASK_GEOMETRY',
    'Luminance Mask origin must be finite',
  );
  check(
    mask.polygons.length > 0 && mask.polygons.length <= LUMINANCE_MASK_LIMITS.contours,
    'LUMINANCE_MASK_GEOMETRY',
    'Luminance Mask needs a bounded set of polygon contours',
  );
  const polygons = [];
  let vertices = 0;
  for (const polygon of mask.polygons) {
    check(
      Array.isArray(polygon) && polygon.length >= 3,
      'LUMINANCE_MASK_GEOMETRY',
      'Luminance Mask polygons need at least three vertices',
    );
    vertices += polygon.length;
    check(
      vertices <= LUMINANCE_MASK_LIMITS.flattenedVertices,
      'LUMINANCE_MASK_LIMIT',
      `Luminance Mask exceeds the ${LUMINANCE_MASK_LIMITS.flattenedVertices}-vertex budget`,
    );
    const integer = polygon.map((point) => {
      check(
        finiteVector(point),
        'LUMINANCE_MASK_GEOMETRY',
        'Luminance Mask contains a non-finite polygon point',
      );
      return integerPoint(point, mask.origin, 'Luminance Mask contour');
    });
    checkNoCollapsedEdges(integer, 'Luminance Mask contour');
    polygons.push(integer);
  }
  return { polygons, vertices };
}

/**
 * Intersect generated world-space polygons with a prepared fill mask and
 * return a standard Nect Path with deterministic contour and point IDs.
 */
export function clipLuminanceMarks(mask, polygons, prefix) {
  const preparedMask = validateMask(mask);
  check(
    Array.isArray(polygons),
    'LUMINANCE_MASK_GEOMETRY',
    'Luminance marks must be an array of polygons',
  );
  check(
    typeof prefix === 'string' && prefix.length > 0,
    'LUMINANCE_MASK_OPTIONS',
    'Luminance mark IDs need a nonempty prefix',
  );

  let generatedVertices = 0;
  const generated = [];
  for (const polygon of polygons) {
    check(
      Array.isArray(polygon) && polygon.length >= 3,
      'LUMINANCE_MASK_GEOMETRY',
      'Each generated luminance mark needs at least three vertices',
    );
    generatedVertices += polygon.length;
    check(
      generatedVertices <= LUMINANCE_MASK_LIMITS.generatedVertices,
      'LUMINANCE_MASK_LIMIT',
      `Generated luminance marks exceed the ${LUMINANCE_MASK_LIMITS.generatedVertices}-vertex budget. Increase Dot spacing or Cell pitch, or reduce dot detail.`,
    );
    const integer = polygon.map((point) => {
      check(
        finiteVector(point),
        'LUMINANCE_MASK_GEOMETRY',
        'Generated luminance marks contain a non-finite point',
      );
      return integerPoint(point, mask.origin, 'Generated luminance mark');
    });
    checkNoCollapsedEdges(integer, 'Generated luminance mark');
    generated.push(integer);
  }
  check(
    preparedMask.vertices * generatedVertices <= LUMINANCE_MASK_LIMITS.pairWork,
    'LUMINANCE_MASK_LIMIT',
    `Luminance Mask intersection exceeds the ${LUMINANCE_MASK_LIMITS.pairWork}-unit work budget. Increase Dot spacing or Cell pitch, or simplify the source path.`,
  );
  if (generated.length === 0) return { contours: [] };

  let solution;
  try {
    solution = intersect(preparedMask.polygons, generated, FillRule.NonZero);
  } catch {
    fail('LUMINANCE_MASK_KERNEL', 'Luminance Mask intersection could not produce a result');
  }
  check(
    solution.length <= LUMINANCE_MASK_LIMITS.outputContours,
    'LUMINANCE_MASK_LIMIT',
    `Clipped luminance marks exceed the ${LUMINANCE_MASK_LIMITS.outputContours}-contour output budget. Increase Dot spacing or Cell pitch, or simplify the source path.`,
  );

  let outputVertices = 0;
  for (const contour of solution) {
    check(
      Array.isArray(contour) && contour.length >= 3,
      'LUMINANCE_MASK_KERNEL',
      'Luminance Mask intersection produced a degenerate contour',
    );
    outputVertices += contour.length;
    check(
      outputVertices <= LUMINANCE_MASK_LIMITS.outputVertices,
      'LUMINANCE_MASK_LIMIT',
      `Clipped luminance marks exceed the ${LUMINANCE_MASK_LIMITS.outputVertices}-point output budget. Increase Dot spacing or Cell pitch, or simplify the source path.`,
    );
    for (const point of contour)
      check(
        Number.isSafeInteger(point.x) &&
          Number.isSafeInteger(point.y) &&
          Math.max(Math.abs(point.x), Math.abs(point.y)) < INTEGER_LIMIT,
        'LUMINANCE_MASK_RANGE',
        'Clipped luminance geometry exceeds the safe integer coordinate envelope',
      );
  }

  return {
    contours: solution.map((contour, contourIndex) => {
      const contourId = `${prefix}:c${contourIndex}`;
      return {
        id: contourId,
        closed: true,
        points: contour.map((point, pointIndex) => {
          const position = [point.x / GRID + mask.origin.x, point.y / GRID + mask.origin.y];
          check(
            position.every(Number.isFinite),
            'LUMINANCE_MASK_RANGE',
            'Clipped luminance geometry contains a non-finite world coordinate',
          );
          return {
            id: `${contourId}:p${pointIndex}`,
            position,
            handleA: [0, 0],
            handleB: [0, 0],
          };
        }),
      };
    }),
  };
}
