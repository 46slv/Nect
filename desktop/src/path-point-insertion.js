// Source-stage geometry only. No document mutation or view-dependent snapping.
const lerp = (a, b, t) => a.map((v, i) => v + (b[i] - v) * t);
const sub = (a, b) => a.map((v, i) => v - b[i]);
export function segmentControls(a, b) {
  return [
    a.position,
    a.position.map((v, i) => v + a.handleB[i]),
    b.position.map((v, i) => v + b.handleA[i]),
    b.position,
  ];
}
export function splitPathSegment(a, b, t) {
  if (!Number.isFinite(t) || t <= 0 || t >= 1) throw Error('Insertion needs 0 < t < 1');
  const [p0, p1, p2, p3] = segmentControls(a, b);
  if ([p1, p2, p3].every((p) => p.every((v, i) => v === p0[i])))
    throw Error('Cannot insert into a fully degenerate segment');
  const q0 = lerp(p0, p1, t),
    q1 = lerp(p1, p2, t),
    q2 = lerp(p2, p3, t),
    r0 = lerp(q0, q1, t),
    r1 = lerp(q1, q2, t),
    position = lerp(r0, r1, t);
  return {
    startHandle: sub(q0, p0),
    endHandle: sub(q2, p3),
    position,
    handleA: sub(r0, position),
    handleB: sub(r1, position),
  };
}
const valueAt = (c, t) => c.reduceRight((v, a) => v * t + a, 0);
// Isolate every stationary-distance root, including multiple roots, rather than
// trusting one nearest sample (which silently chooses a branch at self-crossings).
function roots(c) {
  while (c.length > 1 && Math.abs(c.at(-1)) < 1e-14) c = c.slice(0, -1);
  if (c.length < 2) return [];
  if (c.length === 2) {
    const t = -c[0] / c[1];
    return t > 0 && t < 1 ? [t] : [];
  }
  const cuts = [0, ...roots(c.slice(1).map((v, i) => v * (i + 1))), 1],
    result = [];
  for (const t of cuts) if (t > 0 && t < 1 && Math.abs(valueAt(c, t)) < 1e-12) result.push(t);
  for (let i = 1; i < cuts.length; i++) {
    let lo = cuts[i - 1],
      hi = cuts[i],
      left = valueAt(c, lo);
    if (left * valueAt(c, hi) >= 0) continue;
    for (let j = 0; j < 50; j++) {
      const mid = (lo + hi) / 2,
        v = valueAt(c, mid);
      if (left * v <= 0) hi = mid;
      else {
        lo = mid;
        left = v;
      }
    }
    result.push((lo + hi) / 2);
  }
  return result.sort((a, b) => a - b).filter((t, i, all) => !i || t - all[i - 1] > 1e-7);
}
export function hitPathSegment(path, point, scale, radius = 6) {
  if (!Number.isFinite(scale) || scale <= 0 || !point?.every(Number.isFinite)) return null;
  const hits = [];
  for (const contour of path?.contours || []) {
    const count = contour.points.length;
    for (let i = 0; i < count - (contour.closed ? 0 : 1); i++) {
      const a = contour.points[i],
        b = contour.points[(i + 1) % count],
        controls = segmentControls(a, b);
      if (
        [0, 1].some(
          (axis) =>
            point[axis] < Math.min(...controls.map((p) => p[axis])) - radius / scale ||
            point[axis] > Math.max(...controls.map((p) => p[axis])) + radius / scale,
        )
      )
        continue;
      const norm = Math.max(1, ...controls.flatMap((p) => p.map((v, k) => Math.abs(v - point[k]))));
      const xy = [0, 1].map((axis) => {
        const [p0, p1, p2, p3] = controls.map((p) => (p[axis] - point[axis]) / norm);
        return [p0, 3 * (p1 - p0), 3 * (p2 - 2 * p1 + p0), p3 - 3 * p2 + 3 * p1 - p0];
      });
      const derivative = Array(6).fill(0);
      for (const c of xy)
        for (let j = 0; j < 4; j++)
          for (let k = 1; k < 4; k++) derivative[j + k - 1] += c[j] * k * c[k];
      for (const t of roots(derivative)) {
        if (t < 1e-9 || t > 1 - 1e-9) continue;
        const distance = Math.hypot(...xy.map((c) => valueAt(c, t))) * norm * scale;
        // A local maximum isn't an insertion candidate.
        const epsilon = 1e-6;
        if (
          valueAt(derivative, Math.max(0, t - epsilon)) > 1e-12 ||
          valueAt(derivative, Math.min(1, t + epsilon)) < -1e-12
        )
          continue;
        if (distance <= radius)
          hits.push({
            contourId: contour.id,
            startPointId: a.id,
            endPointId: b.id,
            t,
            position: xy.map((c, axis) => valueAt(c, t) * norm + point[axis]),
            distance,
          });
      }
    }
  }
  if (hits.length > 1) return { ambiguous: true, candidates: hits };
  return hits[0] || null;
}
