export function intersects(a, b, gap = 24) {
  return (
    a.x < b.x + b.width + gap &&
    a.x + a.width + gap > b.x &&
    a.y < b.y + b.height + gap &&
    a.y + a.height + gap > b.y
  );
}
export function findFreeNodePosition(nodes, measurements = {}, size = { width: 264, height: 420 }) {
  const boxes = nodes.map((n) => ({
    x: n.position[0],
    y: n.position[1],
    width: measurements[n.id]?.width || n.width || 264,
    height: measurements[n.id]?.height || 260,
  }));
  for (let row = 0; row < 200; row++)
    for (let column = 0; column < 8; column++) {
      const candidate = { x: 40 + column * 312, y: 40 + row * 312, ...size };
      if (boxes.every((box) => !intersects(candidate, box))) return [candidate.x, candidate.y];
    }
  return [40, Math.max(0, ...boxes.map((b) => b.y + b.height)) + 40];
}

// Shift the entire selection to the right, preserving its internal arrangement.
// A conservative group rectangle also protects empty space between selected cards.
export function adjacentNodeOffset(nodes, ids, measurements = {}) {
  const wanted = new Set(ids);
  const boxes = nodes.map((n) => ({
    id: n.id,
    x: n.position[0],
    y: n.position[1],
    width: measurements[n.id]?.width || n.width || 264,
    height: measurements[n.id]?.height || (n.collapsed ? 44 : n.type === 'Curve' ? 520 : 260),
  }));
  const group = unionBounds(boxes.filter((b) => wanted.has(b.id)));
  if (!group) throw Error('Missing nodes to duplicate');
  const candidate = { ...group, x: group.x + group.width + 24 };
  // Sorted right edges allow a single bounded pass, even in a crowded graph.
  for (const box of boxes.sort((a, b) => a.x + a.width - (b.x + b.width))) {
    if (intersects(candidate, box)) candidate.x = box.x + box.width + 24;
  }
  return [candidate.x - group.x, 0];
}

// View geometry is in CSS pixels. None of these helpers mutate document coordinates.
export function fitScale(bounds, viewport, padding = 32, limits = [0.05, 2]) {
  const available = viewport.map((size) => Math.max(1, size - padding * 2));
  const ideal = Math.min(
    available[0] / Math.max(1, bounds.width),
    available[1] / Math.max(1, bounds.height),
  );
  return { scale: Math.max(limits[0], Math.min(limits[1], ideal)), limited: ideal < limits[0] };
}
export function unionBounds(boxes) {
  if (!boxes.length) return null;
  const x = Math.min(...boxes.map((b) => b.x)),
    y = Math.min(...boxes.map((b) => b.y));
  return {
    x,
    y,
    width: Math.max(...boxes.map((b) => b.x + b.width)) - x,
    height: Math.max(...boxes.map((b) => b.y + b.height)) - y,
  };
}
export function anchoredPan(pan, anchorFromCenter, ratio) {
  return pan.map((value, axis) => value + anchorFromCenter[axis] * (1 - ratio));
}
export function anchoredScroll(scroll, anchor, ratio) {
  return scroll.map((value, axis) => (value + anchor[axis]) * ratio - anchor[axis]);
}
export function wheelPixels(event, viewport) {
  const unit = event.deltaMode === 1 ? [16, 16] : event.deltaMode === 2 ? viewport : [1, 1];
  const delta = [event.deltaX * unit[0], event.deltaY * unit[1]];
  return event.shiftKey && !delta[0] ? [delta[1], 0] : delta;
}

// Graph-only attraction. One group correction per axis, in graph coordinates;
// the tolerance is in screen CSS pixels. Stable IDs break equal-distance ties.
export function snapGraphNodes(moving, targets, delta, zoom, bypass = false, threshold = 6) {
  if (!moving.length) return { delta: [...delta], guides: [] };
  const axes = ['x', 'y'],
    sizes = ['width', 'height'];
  const ids = new Set(moving.map((box) => box.id));
  const ordered = (boxes) => [...boxes].sort((a, b) => String(a.id).localeCompare(String(b.id)));
  // Clamp before searching, then reject candidates that would cross the origin.
  const lowerBounds = axes.map((key) => -Math.min(...moving.map((box) => box[key])));
  const result = delta.map((value, axis) => Math.max(value, lowerBounds[axis]));
  const sources = ordered(moving),
    candidates = ordered(targets.filter((box) => !ids.has(box.id)));
  const matches = [];
  if (!bypass && Number.isFinite(zoom) && zoom > 0) {
    for (let axis = 0; axis < 2; axis++) {
      const key = axes[axis],
        size = sizes[axis];
      let best = null;
      for (const source of sources) {
        for (const target of candidates) {
          for (const a of [0, 0.5, 1])
            for (const b of [0, 0.5, 1]) {
              const coordinate = target[key] + target[size] * b;
              const correction = coordinate - (source[key] + source[size] * a + result[axis]);
              if (
                Math.abs(correction) * zoom > threshold ||
                result[axis] + correction < lowerBounds[axis]
              )
                continue;
              if (!best || Math.abs(correction) < Math.abs(best.correction))
                best = { axis, coordinate, correction, source, target };
            }
        }
      }
      if (best) {
        result[axis] += best.correction;
        matches.push(best);
      }
    }
  }
  return {
    delta: result,
    guides: matches.map(({ axis, coordinate, source, target }) => {
      const cross = 1 - axis,
        key = axes[cross],
        size = sizes[cross];
      return {
        axis,
        coordinate,
        start: Math.min(source[key] + result[cross], target[key]),
        end: Math.max(source[key] + result[cross] + source[size], target[key] + target[size]),
      };
    }),
  };
}
