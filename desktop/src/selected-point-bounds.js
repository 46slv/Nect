import { unionBounds } from './layout.js';

// Derived anchor positions only: no handles, stroke, contour or downstream transform.
export function selectedPointBounds(owner, pointIds, compositionLocked = false) {
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
    boxes = [];
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
    boxes.push({ x: q.value[0], y: q.value[1], width: 0, height: 0 });
  }
  const bounds = unionBounds(boxes);
  if (![bounds.width, bounds.height].every(Number.isFinite))
    return unavailable('Bounds out of range');
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
