import { parameter } from './core.js';
import { artworkAlignments } from './artwork-alignment.js';
import { directSelectionSource, inverseAffineDelta } from './direct-selection.js';

export const anchorAlignments = artworkAlignments.map(([id, , axis, edge]) => [
  id,
  `${axis ? 'Y' : 'X'} ${['min', 'center', 'max'][edge]}`,
  axis,
  edge,
]);

export function pointAlignmentContext(doc, layerId, nodeId, pointIds, options = {}) {
  if (typeof layerId !== 'string' || !layerId || typeof nodeId !== 'string' || !nodeId)
    throw Error('Select an editable source path');
  if (
    !Array.isArray(pointIds) ||
    pointIds.length < 2 ||
    pointIds.length > 10000 ||
    new Set(pointIds).size !== pointIds.length
  )
    throw Error('Select 2-10000 distinct anchors');
  const route = directSelectionSource(doc, layerId, {
    sourceId: nodeId,
    fonts: options.fonts || {},
  });
  if (!route.supported) throw Error(route.reason);
  if (route.locked) throw Error('Point route is locked');
  const n = doc.layers.find((l) => l.id === layerId).nodes.find((n) => n.id === nodeId),
    points = route.path.contours.flatMap((c) => c.points),
    byId = new Map(points.map((p) => [p.id, p]));
  if (byId.size !== points.length) throw Error('Point source identity changed');
  for (const id of pointIds) {
    if (!byId.has(id)) throw Error('Missing selected anchor');
    const q = parameter(n, n.type === 'Path Remap' ? `remap.${id}.position` : `${id}.position`);
    if (q?.locked || q?.source || q?.expression)
      throw Error('Selected position is locked or driven');
    if (!byId.get(id).position.every(Number.isFinite)) throw Error('Invalid selected anchor');
  }
  return {
    matrix: route.matrix,
    points: pointIds.map((id) => byId.get(id)),
    pointOrder: points.map((p) => p.id),
    sourceKey: JSON.stringify([
      layerId,
      nodeId,
      route.route,
      route.matrix,
      points.map((p) => p.id),
      route.path.fingerprint || null,
    ]),
  };
}

export function pointAxisPrecision(context, axis) {
  const norm = Math.abs(context.matrix[axis]) + Math.abs(context.matrix[axis + 2]);
  return Math.max(
    1e-9,
    norm * 1e-10 +
      16 * Number.EPSILON * Math.max(1, ...context.points.map((p) => Math.abs(p.position[axis]))),
  );
}

export function planPointAlignment(
  doc,
  layerId,
  nodeId,
  pointIds,
  action,
  sourceKey,
  options = {},
) {
  const alignment = anchorAlignments.find(([id]) => id === action);
  if (!alignment) throw Error('Choose one of the six anchor alignments');
  const context = pointAlignmentContext(doc, layerId, nodeId, pointIds, options);
  if (typeof sourceKey !== 'string' || sourceKey !== context.sourceKey)
    throw Error('Point alignment cancelled: source changed');
  const [, , axis, edge] = alignment,
    coordinates = context.points.map((p) => p.position[axis]),
    low = Math.min(...coordinates),
    high = Math.max(...coordinates),
    target = [low, low / 2 + high / 2, high][edge],
    precision = pointAxisPrecision(context, axis);
  return context.points.map((p) => {
    const delta = [0, 0],
      distance = target - p.position[axis];
    if (!Number.isFinite(distance)) throw Error('Point alignment exceeds finite range');
    delta[axis] = Math.abs(distance) <= precision ? 0 : distance;
    return { pointId: p.id, delta: inverseAffineDelta(context.matrix, delta) };
  });
}
