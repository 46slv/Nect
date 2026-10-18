import { EndType, FillRule, JoinType, union } from 'clipper2-ts';
import { NectClipperOffset } from './clipper-adapter.js';
import { flattenCubicCenterline } from './closed-offset.js';

const GRID = 1e4;
const GRID_SIZE = 1 / GRID;
const INTEGER_LIMIT = 2 ** 30;
const MAX_CONTOURS = 32;
const MAX_SOURCE_POINTS = 256;
const MAX_FLATTENED_VERTICES = 256;
const MAX_SUBDIVISION_NODES = 4096;
const CENTERLINE_TOLERANCE = 0.1;
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
      worldMagnitude = Math.max(
        worldMagnitude,
        Math.abs(point.position[0]),
        Math.abs(point.position[1]),
      );
      prepared.push({
        x: point.position[0],
        y: point.position[1],
        handleA: [...point.handleA],
        handleB: [...point.handleB],
      });
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

const hasHandle = (handle) => handle[0] !== 0 || handle[1] !== 0;

function controlPoint(anchor, handle) {
  const control = { x: anchor.x + handle[0], y: anchor.y + handle[1] };
  if (
    !Number.isFinite(control.x) ||
    !Number.isFinite(control.y) ||
    (handle[0] !== 0 && control.x === anchor.x) ||
    (handle[1] !== 0 && control.y === anchor.y)
  )
    fail('Expand Stroke cannot preserve a curve handle at the local coordinate precision');
  return control;
}

function endpointTangentMarker(controls, start) {
  if (start) {
    for (const point of controls.slice(1)) if (!samePoint(point, controls[0])) return point;
  } else {
    for (const point of controls.slice(0, 3).reverse())
      if (!samePoint(point, controls[3])) return point;
  }
  fail('Expand Stroke rejects a zero-length curve endpoint tangent');
}

function flattenSourceGroups(groups) {
  const hasCurves = groups.some((contours) =>
    contours.some((contour) =>
      contour.points.some((point) => hasHandle(point.handleA) || hasHandle(point.handleB)),
    ),
  );
  // Keep the established straight-input path, grid order and Clipper calls
  // exactly unchanged when every authored handle is zero.
  if (!hasCurves)
    return groups.map((contours) =>
      contours.map((contour) => ({
        closed: contour.closed,
        points: contour.points.map(({ x, y }) => ({ x, y })),
      })),
    );

  let subdivisionNodes = 0;
  let curvedLeaves = 0;
  let flattenedVertices = 0;
  const result = groups.map((contours) => {
    const nonempty = contours.find((contour) => contour.points.length);
    const origin = nonempty ? nonempty.points[0] : { x: 0, y: 0 };
    const prepared = contours.map((contour) => {
      const anchors = contour.points.map((point) => ({
        x: point.x - origin.x,
        y: point.y - origin.y,
      }));
      const segments = [];
      let maxLocal = 0;
      for (const anchor of anchors)
        maxLocal = Math.max(maxLocal, Math.abs(anchor.x), Math.abs(anchor.y));
      const edgeCount = contour.closed ? anchors.length : Math.max(0, anchors.length - 1);
      for (let i = 0; i < edgeCount; i++) {
        const nextIndex = (i + 1) % anchors.length;
        const currentSource = contour.points[i];
        const nextSource = contour.points[nextIndex];
        const a = anchors[i];
        const d = anchors[nextIndex];
        const b = controlPoint(a, currentSource.handleB);
        const c = controlPoint(d, nextSource.handleA);
        for (const point of [b, c])
          maxLocal = Math.max(maxLocal, Math.abs(point.x), Math.abs(point.y));
        segments.push({
          controls: [a, b, c, d],
          curved: hasHandle(currentSource.handleB) || hasHandle(nextSource.handleA),
        });
      }
      const floatGuard =
        256 * Number.EPSILON * Math.max(1, maxLocal, Math.abs(origin.x), Math.abs(origin.y));
      if (floatGuard >= CENTERLINE_TOLERANCE / 4)
        fail('Expand Stroke world coordinates are too large for 0.1 px curve flattening');

      const points = [];
      let tangentStart = null;
      let tangentEnd = null;
      let firstMarker = null;
      let lastMarker = null;
      for (let i = 0; i < segments.length; i++) {
        const segment = segments[i];
        if (!segment.curved) {
          points.push(segment.controls[0]);
          continue;
        }
        const remainingNodes = MAX_SUBDIVISION_NODES - subdivisionNodes;
        const remainingVertices = MAX_FLATTENED_VERTICES - curvedLeaves;
        // Tangent support points become vertices in the open endpoint leaves.
        // Halving those leaves' flatness budget keeps the support polyline within
        // the documented 0.1 px centerline tolerance as well.
        const endpointSegment = !contour.closed && (i === 0 || i === segments.length - 1);
        const flattenTolerance = CENTERLINE_TOLERANCE / (endpointSegment ? 2 : 1) - floatGuard;
        let flattened;
        try {
          flattened = flattenCubicCenterline(segment.controls, {
            tolerance: flattenTolerance,
            floatGuard,
            maxNodes: remainingNodes,
            maxVertices: remainingVertices,
          });
        } catch (error) {
          if (error?.code === 'OFFSET_LIMIT')
            fail(
              `Expand Stroke curve flattening exceeds its 256-vertex, 4096-node, or 24-level budget`,
            );
          throw error;
        }
        subdivisionNodes += flattened.subdivisionNodes;
        curvedLeaves += flattened.flattenedVertices;
        points.push(...flattened.leaves.map(({ controls: leaf }) => leaf[0]));
        if (!contour.closed && i === 0) {
          const marker = endpointTangentMarker(flattened.leaves[0].controls, true);
          firstMarker = marker;
          tangentStart = { x: marker.x - anchors[0].x, y: marker.y - anchors[0].y };
        }
        if (!contour.closed && i === segments.length - 1) {
          const marker = endpointTangentMarker(flattened.leaves.at(-1).controls, false);
          lastMarker = marker;
          const endpoint = anchors.at(-1);
          tangentEnd = { x: endpoint.x - marker.x, y: endpoint.y - marker.y };
        }
      }
      if (!contour.closed && anchors.length) points.push(anchors.at(-1));
      if (firstMarker && points.length > 1 && !samePoint(firstMarker, points[1]))
        points.splice(1, 0, firstMarker);
      if (lastMarker && points.length > 1 && !samePoint(lastMarker, points.at(-2)))
        points.splice(points.length - 1, 0, lastMarker);
      flattenedVertices += points.length;
      if (flattenedVertices > MAX_FLATTENED_VERTICES)
        fail(
          `Expand Stroke flattened centerline exceeds the ${MAX_FLATTENED_VERTICES}-vertex budget`,
        );
      return { closed: contour.closed, points, tangentStart, tangentEnd };
    });
    prepared.origin = { x: origin.x, y: origin.y };
    return prepared;
  });
  return result;
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
  const origin = contours.origin ?? localOrigin(contours);
  const coordinateOrigin = contours.origin ? { x: 0, y: 0 } : origin;
  const radius = width / 2;
  const reach = radius * (join === 'miter' ? miterLimit : 1) + (cap === 'square' ? radius : 0);
  let maxLocal = 0;
  for (const contour of contours)
    for (const point of contour.points) {
      maxLocal = Math.max(
        maxLocal,
        Math.abs(point.x - coordinateOrigin.x),
        Math.abs(point.y - coordinateOrigin.y),
      );
    }
  if ((maxLocal + reach + GRID_SIZE) * GRID >= INTEGER_LIMIT)
    fail('Expand Stroke local geometry and width/miter reach exceed the safe integer envelope');

  const paths = contours.map((contour) => {
    const points = contour.points.map((point) => ({
      x: point.x - coordinateOrigin.x,
      y: point.y - coordinateOrigin.y,
    }));
    if (!contour.closed && cap === 'square') {
      const firstUnit = contour.tangentStart
        ? lineUnit({ x: 0, y: 0 }, contour.tangentStart)
        : lineUnit(points[0], points[1]);
      const lastUnit = contour.tangentEnd
        ? lineUnit({ x: 0, y: 0 }, contour.tangentEnd)
        : lineUnit(points.at(-2), points.at(-1));
      points[0] = {
        x: points[0].x - firstUnit.x * radius,
        y: points[0].y - firstUnit.y * radius,
      };
      points[points.length - 1] = {
        x: points.at(-1).x + lastUnit.x * radius,
        y: points.at(-1).y + lastUnit.y * radius,
      };
    }
    const quantized = points.map((point) => pointToGrid(point, { x: 0, y: 0 }));
    if (
      !contour.closed &&
      ((contour.tangentStart && samePoint(quantized[0], quantized[1])) ||
        (contour.tangentEnd && samePoint(quantized.at(-2), quantized.at(-1))))
    )
      fail('Expand Stroke endpoint tangent collapses at the 0.0001 px geometry grid');
    let gridPoints = dropDuplicateGridPoints(quantized, contour.closed);
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

const cross = (a, b) => a.x * b.y - a.y * b.x;
const dot = (a, b) => a.x * b.x + a.y * b.y;
const subtract = (a, b) => ({ x: a.x - b.x, y: a.y - b.y });
const add = (a, b) => ({ x: a.x + b.x, y: a.y + b.y });
const scale = (point, factor) => ({ x: point.x * factor, y: point.y * factor });

function validateWidthMultipliers(options) {
  const start = options.startWidthMultiplier === undefined ? 1 : options.startWidthMultiplier;
  const end = options.endWidthMultiplier === undefined ? 1 : options.endWidthMultiplier;
  if (
    !Number.isFinite(start) ||
    start < 0 ||
    start > 4 ||
    !Number.isFinite(end) ||
    end < 0 ||
    end > 4
  )
    fail('Expand Stroke start and end width multipliers must be finite and between 0 and 4');
  return { start, end };
}

function roundStepAngle(radius) {
  if (!(radius > 0) || !Number.isFinite(radius)) return null;
  const tolerance = Math.min(0.1, radius / 4);
  const stepAngle = Math.acos(1 - tolerance / radius);
  if (!Number.isFinite(stepAngle) || stepAngle <= 0)
    fail('Expand Stroke cannot estimate round join work at this width');
  return stepAngle;
}

function appendRoundArc(output, center, radius, startAngle, sweep, work) {
  if (radius === 0) return;
  const stepAngle = roundStepAngle(radius);
  const steps = Math.max(1, Math.ceil(Math.abs(sweep) / stepAngle));
  work.count += steps;
  if (work.count > MAX_ROUND_WORK)
    fail(`Expand Stroke round joins exceed the ${MAX_ROUND_WORK}-vertex work budget`);
  for (let step = 1; step <= steps; step++) {
    const angle = startAngle + (sweep * step) / steps;
    output.push({
      x: center.x + Math.cos(angle) * radius,
      y: center.y + Math.sin(angle) * radius,
    });
  }
}

function appendVariableSide(output, centers, radii, directions, side, stroke, work) {
  const normal = (direction) => ({ x: -direction.y * side, y: direction.x * side });
  const startOffset = scale(normal(directions[0]), radii[0]);
  output.push(add(centers[0], startOffset));
  for (let index = 1; index < centers.length - 1; index++) {
    const center = centers[index];
    const incoming = directions[index - 1];
    const outgoing = directions[index];
    const turn = cross(incoming, outgoing);
    const alignment = dot(incoming, outgoing);
    if (Math.abs(turn) < 1e-8) {
      if (alignment < 0)
        fail('Expand Stroke rejects a cusp that reverses the centerline direction');
      const incomingPoint = add(center, scale(normal(incoming), radii[index]));
      const outgoingPoint = add(center, scale(normal(outgoing), radii[index]));
      output.push(incomingPoint, outgoingPoint);
      continue;
    }

    const incomingPoint = add(center, scale(normal(incoming), radii[index]));
    const outgoingPoint = add(center, scale(normal(outgoing), radii[index]));
    const isOuter = turn * side < 0;
    if (isOuter && stroke.join === 'bevel') {
      output.push(incomingPoint, outgoingPoint);
    } else if (isOuter && stroke.join === 'round') {
      output.push(incomingPoint);
      const startAngle = Math.atan2(incomingPoint.y - center.y, incomingPoint.x - center.x);
      const endAngle = Math.atan2(outgoingPoint.y - center.y, outgoingPoint.x - center.x);
      const sweepSign = -Math.sign(side);
      let sweep = endAngle - startAngle;
      if (sweepSign > 0) while (sweep <= 0) sweep += 2 * Math.PI;
      else while (sweep >= 0) sweep -= 2 * Math.PI;
      appendRoundArc(output, center, radii[index], startAngle, sweep, work);
    } else {
      const intersectionDistance = cross(subtract(outgoingPoint, incomingPoint), outgoing) / turn;
      const intersection = add(incomingPoint, scale(incoming, intersectionDistance));
      const miterReach = Math.hypot(intersection.x - center.x, intersection.y - center.y);
      if (!Number.isFinite(miterReach))
        fail('Expand Stroke variable-width join exceeds its bounded miter reach');
      if (miterReach > radii[index] * stroke.miterLimit) output.push(incomingPoint, outgoingPoint);
      else output.push(intersection);
    }
  }
  const endOffset = scale(normal(directions.at(-1)), radii.at(-1));
  output.push(add(centers.at(-1), endOffset));
}

function prepareVariableGroup(
  contours,
  stroke,
  startMultiplier,
  endMultiplier,
  work = { count: 0 },
) {
  const origin = contours.origin ?? localOrigin(contours);
  const coordinateOrigin = contours.origin ? { x: 0, y: 0 } : origin;
  const maxWidth = stroke.width * Math.max(startMultiplier, endMultiplier);
  const maxRadius = maxWidth / 2;
  const reach = maxRadius * stroke.miterLimit + (stroke.cap === 'square' ? maxRadius : 0);
  let maxLocal = 0;
  for (const contour of contours)
    for (const point of contour.points)
      maxLocal = Math.max(
        maxLocal,
        Math.abs(point.x - coordinateOrigin.x),
        Math.abs(point.y - coordinateOrigin.y),
      );
  if ((maxLocal + reach + GRID_SIZE) * GRID >= INTEGER_LIMIT)
    fail('Expand Stroke local geometry and variable-width reach exceed the safe integer envelope');

  const outlines = [];
  for (const contour of contours) {
    if (contour.closed) fail('Expand Stroke variable-width strokes support open contours only');
    const quantized = contour.points.map((point) =>
      pointToGrid(
        { x: point.x - coordinateOrigin.x, y: point.y - coordinateOrigin.y },
        { x: 0, y: 0 },
      ),
    );
    if (
      (contour.tangentStart && samePoint(quantized[0], quantized[1])) ||
      (contour.tangentEnd && samePoint(quantized.at(-2), quantized.at(-1)))
    )
      fail('Expand Stroke endpoint tangent collapses at the 0.0001 px geometry grid');
    const gridPoints = dropDuplicateGridPoints(quantized, false);
    if (gridPoints.length < 2)
      fail('Expand Stroke source edges collapse at the 0.0001 px geometry grid');
    const centers = gridPoints.map((point) => ({ x: point.x / GRID, y: point.y / GRID }));
    const directions = [];
    const lengths = [];
    let totalLength = 0;
    for (let index = 0; index < centers.length - 1; index++) {
      const dx = centers[index + 1].x - centers[index].x;
      const dy = centers[index + 1].y - centers[index].y;
      const length = Math.hypot(dx, dy);
      if (!Number.isFinite(length) || length === 0)
        fail('Expand Stroke source edges collapse at the 0.0001 px geometry grid');
      directions.push({ x: dx / length, y: dy / length });
      lengths.push(length);
      totalLength += length;
    }
    if (!(totalLength > 0) || !Number.isFinite(totalLength))
      fail('Expand Stroke centerline has no finite arc length');

    const radii = [(stroke.width * startMultiplier) / 2];
    let traveled = 0;
    for (const length of lengths) {
      traveled += length;
      const fraction = traveled / totalLength;
      const multiplier = startMultiplier + (endMultiplier - startMultiplier) * fraction;
      radii.push((stroke.width * multiplier) / 2);
    }

    const left = [];
    const right = [];
    appendVariableSide(left, centers, radii, directions, 1, stroke, work);
    appendVariableSide(right, centers, radii, directions, -1, stroke, work);
    const outline = [...left];
    if (stroke.cap === 'round') {
      const endCenter = centers.at(-1);
      const endNormal = { x: -directions.at(-1).y, y: directions.at(-1).x };
      const startAngle = Math.atan2(endNormal.y, endNormal.x);
      appendRoundArc(outline, endCenter, radii.at(-1), startAngle, -Math.PI, work);
    } else if (stroke.cap === 'square') {
      const extension = scale(directions.at(-1), radii.at(-1));
      const normalExtension = scale(
        { x: -directions.at(-1).y, y: directions.at(-1).x },
        radii.at(-1),
      );
      outline.push(
        add(add(centers.at(-1), extension), normalExtension),
        add(add(centers.at(-1), extension), scale(normalExtension, -1)),
      );
    } else outline.push(right.at(-1));
    for (let index = right.length - 2; index >= 0; index--) outline.push(right[index]);
    if (stroke.cap === 'round') {
      const startCenter = centers[0];
      const startNormal = { x: -directions[0].y, y: directions[0].x };
      const rightAngle = Math.atan2(-startNormal.y, -startNormal.x);
      appendRoundArc(outline, startCenter, radii[0], rightAngle, -Math.PI, work);
    } else if (stroke.cap === 'square') {
      const extension = scale(directions[0], -radii[0]);
      const normalExtension = scale({ x: -directions[0].y, y: directions[0].x }, radii[0]);
      outline.push(
        add(add(centers[0], extension), scale(normalExtension, -1)),
        add(add(centers[0], extension), normalExtension),
      );
    }

    const quantizedOutline = dropDuplicateGridPoints(
      outline.map((point) => pointToGrid(point, { x: 0, y: 0 })),
      true,
    );
    if (quantizedOutline.length < 3)
      fail('Expand Stroke variable-width outline collapses at the 0.0001 px geometry grid');
    outlines.push(quantizedOutline);
  }
  return { origin, outlines };
}

/**
 * Convert a solid Stroke result to unpainted filled geometry. Cubic centerlines
 * are flattened with bounded adaptive subdivision before offsetting. Optional
 * start/end multipliers linearly ramp widths by normalized flattened arc length
 * on open contours; equal multipliers use the existing constant-width kernel.
 * Unequal profiles reject closed contours; zero-radius caps collapse to tips,
 * and an all-zero profile returns empty geometry. Input groups are cleaned
 * independently. Reversing cusps reject, and joins fall back to bevel at the
 * configured miter reach. Existing contour, flatten, round-work and output
 * budgets remain in force. The 0.1 px tolerance applies to the centerline only,
 * not the final outline. Straight default-width input retains its original fast path.
 */
export function expandStrokeGeometry(source, options = {}) {
  if (!options || typeof options !== 'object' || Array.isArray(options))
    fail('Expand Stroke needs valid options');
  const { prefix = 'expandStroke', startWidthMultiplier = 1, endWidthMultiplier = 1 } = options;
  if (typeof prefix !== 'string' || !prefix || prefix.length > 256)
    fail('Expand Stroke needs a nonempty output ID prefix of at most 256 characters');
  const multipliers = validateWidthMultipliers({ startWidthMultiplier, endWidthMultiplier });
  const stroke = validateStroke(source?.stroke);
  const groups = flattenSourceGroups(validateSource(source));
  const uniformMultiplier = multipliers.start === multipliers.end;
  if (!uniformMultiplier && groups.some((group) => group.some((contour) => contour.closed)))
    fail('Expand Stroke variable-width strokes support open contours only');
  const maxWidth = stroke.width * Math.max(multipliers.start, multipliers.end);
  if (stroke.width === 0 || maxWidth === 0 || groups.every((group) => group.length === 0))
    return { contours: [] };
  if (maxWidth * GRID < 2)
    fail('Nonzero Expand Stroke width must be at least 0.0002 px at the 0.0001 px geometry grid');

  const expansionStroke =
    uniformMultiplier && multipliers.start !== 1
      ? { ...stroke, width: stroke.width * multipliers.start }
      : stroke;

  if (uniformMultiplier) {
    const roundWork = groups.reduce(
      (sum, contours) =>
        sum +
        roundArcEstimate(
          contours,
          expansionStroke.cap,
          expansionStroke.join,
          expansionStroke.width / 2,
        ),
      0,
    );
    if (roundWork > MAX_ROUND_WORK)
      fail(`Expand Stroke round joins exceed the ${MAX_ROUND_WORK}-vertex work budget`);
  }

  const joinTypes = { miter: JoinType.Miter, bevel: JoinType.Bevel, round: JoinType.Round };
  const endTypes = { butt: EndType.Butt, square: EndType.Butt, round: EndType.Round };
  const variableRoundWork = { count: 0 };
  const preparedGroups = groups.map((group) =>
    group.length
      ? uniformMultiplier
        ? prepareGroup(group, expansionStroke)
        : prepareVariableGroup(group, stroke, multipliers.start, multipliers.end, variableRoundWork)
      : null,
  );
  const contours = [];
  let outputPointCount = 0;
  for (const [groupIndex, prepared] of preparedGroups.entries()) {
    if (!prepared) continue;
    const { origin } = prepared;
    let solution;
    if (uniformMultiplier) {
      const { paths } = prepared;
      const engine = new NectClipperOffset(
        expansionStroke.miterLimit,
        Math.min(0.1, expansionStroke.width / 8) * GRID,
      );
      const openPaths = paths.filter((path) => !path.closed).map((path) => path.points);
      const closedPaths = paths.filter((path) => path.closed).map((path) => path.points);
      // Joined deliberately treats a closed source contour as a centered stroke,
      // producing outer and inner boundaries. Polygon would expand only outward.
      engine.addPaths(openPaths, joinTypes[expansionStroke.join], endTypes[expansionStroke.cap]);
      engine.addPaths(closedPaths, joinTypes[expansionStroke.join], EndType.Joined);
      solution = [];
      try {
        engine.execute((expansionStroke.width / 2) * GRID, solution);
      } catch {
        fail('Expand Stroke offset kernel could not produce a result');
      }
    } else {
      try {
        solution = union(prepared.outlines, FillRule.NonZero);
      } catch {
        fail('Expand Stroke variable-width outline cleanup could not produce a result');
      }
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
