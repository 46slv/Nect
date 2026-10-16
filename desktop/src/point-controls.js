// View-only hit testing in screen pixels. Derived point IDs remain core-owned.
export function pointControlTargets(path, scale, focused = []) {
  if (!Number.isFinite(scale) || scale <= 0) return [];
  const shown = new Set(focused.filter(Boolean));
  return (path?.contours || []).flatMap((c) =>
    c.points.flatMap((p) => {
      const targets = [{ pointId: p.id, key: 'position', position: p.position, radius: 7 }];
      if (shown.has(p.id))
        for (const key of ['handleA', 'handleB']) {
          if (Math.hypot(...p[key]) * scale < 1) continue;
          targets.push({
            pointId: p.id,
            key,
            position: p.position.map((v, i) => v + p[key][i]),
            radius: 6,
          });
        }
      return targets;
    }),
  );
}
export function hitPointControl(path, point, scale, focused = []) {
  const hits = pointControlTargets(path, scale, focused)
    .map((target) => ({
      ...target,
      distance: Math.hypot(...point.map((v, i) => v - target.position[i])) * scale,
    }))
    .filter((t) => t.distance <= t.radius)
    .sort((a, b) => a.distance - b.distance || (a.key === 'position' ? -1 : 1));
  if (!hits.length) return null;
  if (
    hits[1] &&
    Math.abs(hits[0].distance - hits[1].distance) < 0.25 &&
    Math.hypot(...hits[0].position.map((v, i) => v - hits[1].position[i])) * scale < 0.5 &&
    hits[0].pointId !== hits[1].pointId
  )
    return { ambiguous: true };
  return hits[0];
}
