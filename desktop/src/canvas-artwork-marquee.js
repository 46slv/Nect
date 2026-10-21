// Whole rendered layer bounds, never source-node or anchor geometry.
export function artworkMarqueeIds(targets, start, end, initial = []) {
  if (![...start, ...end].every(Number.isFinite)) return [...initial];
  const low = start.map((v, i) => Math.min(v, end[i]));
  const high = start.map((v, i) => Math.max(v, end[i]));
  const hits = targets.filter(({ bounds }) =>
    bounds.every(
      (axis, i) => axis.every(Number.isFinite) && axis[2] >= low[i] && axis[0] <= high[i],
    ),
  );
  return [...new Set([...initial, ...hits.map((target) => target.id)])];
}
