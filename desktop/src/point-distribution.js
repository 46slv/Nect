import { pointAlignmentContext, pointAxisPrecision } from './point-alignment.js';
import { inverseAffineDelta } from './direct-selection.js';

export const anchorDistributions = [
  ['x', 'Horizontal', 0],
  ['y', 'Vertical', 1],
];

function spacingContext(doc, layerId, nodeId, pointIds, action, sourceKey, options = {}) {
  const distribution = anchorDistributions.find(([id]) => id === action);
  if (!distribution) throw Error('Choose horizontal or vertical anchor spacing');
  const context = pointAlignmentContext(doc, layerId, nodeId, pointIds, options);
  if (typeof sourceKey !== 'string' || sourceKey !== context.sourceKey)
    throw Error('Point spacing cancelled: source changed');
  const axis = distribution[2],
    order = new Map(context.pointOrder.map((id, i) => [id, i])),
    points = context.points.toSorted(
      (a, b) => a.position[axis] - b.position[axis] || order.get(a.id) - order.get(b.id),
    ),
    low = points[0].position[axis];
  return { context, axis, points, low };
}

function spacingMoves({ context, axis, points, low }, step, fixedEnd) {
  if (!Number.isFinite(step)) throw Error('Point spacing exceeds finite range');
  if (!Number.isFinite(low + step * (points.length - 1)))
    throw Error('Point spacing exceeds finite range');
  const precision = pointAxisPrecision(context, axis);
  return points.map((point, i) => {
    const delta = [0, 0];
    // Preserve endpoint authored values exactly, without a round-trip through the inverse.
    if (i === 0 || (fixedEnd && i === points.length - 1)) return { pointId: point.id, delta };
    const distance = low + step * i - point.position[axis];
    if (!Number.isFinite(distance)) throw Error('Point spacing exceeds finite range');
    delta[axis] = Math.abs(distance) <= precision ? 0 : distance;
    return { pointId: point.id, delta: inverseAffineDelta(context.matrix, delta) };
  });
}

export function planPointDistribution(
  doc,
  layerId,
  nodeId,
  pointIds,
  action,
  sourceKey,
  options = {},
) {
  if (!Array.isArray(pointIds) || pointIds.length < 3)
    throw Error('Select at least 3 distinct anchors');
  const context = spacingContext(doc, layerId, nodeId, pointIds, action, sourceKey, options);
  return spacingMoves(
    context,
    (context.points.at(-1).position[context.axis] - context.low) / (context.points.length - 1),
    true,
  );
}

export function planPointSpacing(doc, layerId, nodeId, pointIds, action, sourceKey, options = {}) {
  if (
    typeof options.spacing !== 'number' ||
    !Number.isFinite(options.spacing) ||
    options.spacing < 0
  )
    throw Error('Enter a finite, non-negative anchor spacing');
  return spacingMoves(
    spacingContext(doc, layerId, nodeId, pointIds, action, sourceKey, options),
    options.spacing,
    false,
  );
}
