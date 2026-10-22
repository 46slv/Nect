import { unionBounds } from './layout.js';
import { directSelectionSource } from './direct-selection.js';

function selectedOwnedPositions(owner, pointIds, compositionLocked) {
  const ids = [...pointIds],
    count = ids.length;
  const unavailable = (reason) => ({ state: 'unavailable', count, reason });
  if (!count) return { state: 'empty', count, reason: 'No points selected' };
  if (!owner || !['Curve', 'Multi Path'].includes(owner.type))
    return unavailable('Owned points only');
  if (compositionLocked || owner.locked) return unavailable('Locked selection');
  if (owner.bypassed) return unavailable('Bypassed source');
  if (new Set(ids).size !== count) return unavailable('Selection changed');
  const points = new Map(owner.points.map((p) => [p.id, p])),
    positions = [];
  for (const id of ids) {
    const p = points.get(id);
    if (!p) return unavailable('Selection changed');
    const q = p.position;
    if (q?.locked) return unavailable('Locked position');
    if (q?.source || q?.expression) return unavailable('Driven position');
    if (
      q?.type !== 'Vector2' ||
      !Array.isArray(q.value) ||
      q.value.length !== 2 ||
      !q.value.every(Number.isFinite)
    )
      return unavailable('Invalid position');
    positions.push(q.value);
  }
  return { state: 'ready', count, positions };
}

function positionBounds(positions, count) {
  const boxes = positions.map(([x, y]) => ({ x, y, width: 0, height: 0 }));
  const bounds = unionBounds(boxes);
  if (![bounds.width, bounds.height].every(Number.isFinite))
    return { state: 'unavailable', count, reason: 'Bounds out of range' };
  return {
    state: 'ready',
    count,
    values: {
      minX: bounds.x,
      maxX: Math.max(...boxes.map((b) => b.x)),
      width: bounds.width,
      minY: bounds.y,
      maxY: Math.max(...boxes.map((b) => b.y)),
      height: bounds.height,
    },
  };
}

// Derived anchor positions only: no handles, stroke, contour or downstream transform.
export function selectedPointBounds(owner, pointIds, compositionLocked = false) {
  const selected = selectedOwnedPositions(owner, pointIds, compositionLocked);
  return selected.state === 'ready' ? positionBounds(selected.positions, selected.count) : selected;
}

// Use the same supported Artboard route as point alignment/distribution. This
// read-only query never bakes a source or expands direct-selection support.
export function selectedPointArtboardBounds(doc, layerId, nodeId, pointIds, options = {}) {
  const ids = [...pointIds],
    layer = doc.layers.find((l) => l.id === layerId),
    owner = layer?.nodes.find((n) => n.id === nodeId),
    selected = selectedOwnedPositions(owner, ids, layer?.locked);
  if (selected.state !== 'ready') return selected;
  const unavailable = (reason) => ({ state: 'unavailable', count: ids.length, reason }),
    route = directSelectionSource(doc, layerId, { sourceId: nodeId, fonts: options.fonts || {} });
  if (!route.supported) return unavailable(route.reason);
  if (route.locked) return unavailable('Point route is locked');
  const points = route.path.contours.flatMap((c) => c.points),
    byId = new Map(points.map((p) => [p.id, p]));
  if (byId.size !== points.length) return unavailable('Point source identity changed');
  const positions = [];
  for (const id of ids) {
    const point = byId.get(id);
    if (!point) return unavailable('Missing selected anchor');
    if (!point.position.every(Number.isFinite)) return unavailable('Invalid selected anchor');
    positions.push(point.position);
  }
  return positionBounds(positions, ids.length);
}
