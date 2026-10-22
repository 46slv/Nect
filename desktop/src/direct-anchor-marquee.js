// Targets are the displayed, artboard-space point controls, never source-local
// authored points or layer/segment bounds. Preserve their stable source order.
export function anchorMarqueeIds(targets, start, end, initial = []) {
  const anchors = targets.filter((t) => t.key === 'position'),
    selected = new Set(initial);
  if ([...start, ...end].every(Number.isFinite)) {
    const low = start.map((v, i) => Math.min(v, end[i])),
      high = start.map((v, i) => Math.max(v, end[i]));
    for (const t of anchors)
      if (t.position.every((v, i) => Number.isFinite(v) && v >= low[i] && v <= high[i]))
        selected.add(t.pointId);
  }
  return [...new Set(anchors.filter((t) => selected.has(t.pointId)).map((t) => t.pointId))];
}
