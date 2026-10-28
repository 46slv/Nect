// Bounded vector-domain rectangular/polar conversion. Cubic controls are mapped
// independently; this is not a pixel filter or an exact nonlinear cubic warp.
const TAU = Math.PI * 2;
const vector = (v) => Array.isArray(v) && v.length === 2 && v.every(Number.isFinite);
const finite = (v) => {
  if (!Number.isFinite(v) || Math.abs(v) > 1e9)
    throw Error('Polar Transform exceeds finite coordinate limits');
  return v;
};

export function polarSettings(options) {
  const { origin, center, width, height, radius, angle, inverse } = options;
  if (
    !vector(origin) ||
    !vector(center) ||
    [...origin, ...center].some((v) => Math.abs(v) > 100000)
  )
    throw Error('Polar Transform needs finite origin and center within ±100000 px');
  if ([width, height, radius].some((v) => !Number.isFinite(v) || v < 0.0001 || v > 100000))
    throw Error('Polar Transform width, height and radius must be 0.0001–100000 px');
  if (!Number.isFinite(angle) || Math.abs(angle) > 360)
    throw Error('Polar Transform angle must be within ±360 degrees');
  if (typeof inverse !== 'boolean') throw Error('Polar Transform inverse must be Boolean');
  return { origin: [...origin], center: [...center], width, height, radius, angle, inverse };
}

function mapPoint(position, o) {
  if (!vector(position)) throw Error('Polar Transform needs finite coordinates');
  if (!o.inverse) {
    const u = finite((position[0] - o.origin[0]) / o.width);
    const r = finite(((position[1] - o.origin[1]) / o.height) * o.radius);
    if (r < 0) throw Error('Polar Transform does not support negative radii; move the origin');
    const theta = (u % 1) * TAU + (o.angle * Math.PI) / 180;
    return [finite(o.center[0] + r * Math.cos(theta)), finite(o.center[1] + r * Math.sin(theta))];
  }
  const dx = finite(position[0] - o.center[0]),
    dy = finite(position[1] - o.center[1]);
  const r = finite(Math.hypot(dx, dy));
  const theta = r === 0 ? 0 : Math.atan2(dy, dx) - (o.angle * Math.PI) / 180;
  const u = (((theta / TAU) % 1) + 1) % 1;
  return [finite(o.origin[0] + u * o.width), finite(o.origin[1] + (r / o.radius) * o.height)];
}

// A cubic can cross the seam twice even when both anchors and their own
// handles pass the angular-distance checks. De Casteljau subdivision bounds
// the entire curve by its control hull; reject ambiguous hulls at the limit.
function assertInverseSegment(points, o) {
  const angle = (o.angle * Math.PI) / 180,
    cos = Math.cos(angle),
    sin = Math.sin(angle);
  const rotated = points.map(([x, y]) => {
    const dx = finite(x - o.center[0]),
      dy = finite(y - o.center[1]);
    return [dx * cos + dy * sin, dy * cos - dx * sin];
  });
  let budget = 512;
  const safe = (p, depth) => {
    if (p.every((v) => v[0] < 0) || p.every((v) => v[1] >= 0) || p.every((v) => v[1] < 0))
      return true;
    if (!depth || --budget <= 0) return false;
    const midpoint = (a, b) => a.map((v, i) => (v + b[i]) / 2);
    const a = midpoint(p[0], p[1]),
      b = midpoint(p[1], p[2]),
      c = midpoint(p[2], p[3]);
    const d = midpoint(a, b),
      e = midpoint(b, c),
      f = midpoint(d, e);
    return safe([p[0], a, d, f], depth - 1) && safe([f, e, c, p[3]], depth - 1);
  };
  if (!safe(rotated, 16))
    throw Error(
      'Polar Transform inverse crosses or approaches the angle seam; split or rotate the source',
    );
}

export function polarTransformGeometry(source, options) {
  const o = polarSettings(options);
  if (!source || !Array.isArray(source.contours)) throw Error('Connect a Path input');
  if (source.contours.length > 10000) throw Error('Polar Transform exceeds geometry budget');
  let count = 0;
  const copy = structuredClone(source);
  for (const contour of copy.contours) {
    if (!Array.isArray(contour.points) || typeof contour.closed !== 'boolean')
      throw Error('Polar Transform needs valid contours');
    count += contour.points.length;
    if (count > 100000) throw Error('Polar Transform exceeds geometry budget');
    for (const p of contour.points) {
      if (!vector(p.position) || !vector(p.handleA) || !vector(p.handleB))
        throw Error('Polar Transform needs finite anchors and handles');
    }
    if (o.inverse) {
      const pts = contour.points;
      for (let i = 0; i < pts.length - (contour.closed ? 0 : 1); i++) {
        const p = pts[i],
          next = pts[(i + 1) % pts.length];
        assertInverseSegment(
          [
            p.position,
            p.position.map((v, axis) => finite(v + p.handleB[axis])),
            next.position.map((v, axis) => finite(v + next.handleA[axis])),
            next.position,
          ],
          o,
        );
      }
    }
    for (const p of contour.points) {
      const position = mapPoint(p.position, o);
      for (const key of ['handleA', 'handleB']) {
        const mapped = mapPoint(
          p.position.map((v, i) => finite(v + p[key][i])),
          o,
        );
        if (o.inverse && Math.abs(mapped[0] - position[0]) > o.width / 2)
          throw Error('Polar Transform inverse crosses the angle seam; split or rotate the source');
        p[key] = mapped.map((v, i) => finite(v - position[i]));
      }
      p.position = position;
    }
    if (o.inverse) {
      const pts = contour.points;
      for (let i = 1; i < pts.length + (contour.closed && pts.length > 1 ? 1 : 0); i++) {
        if (Math.abs(pts[i % pts.length].position[0] - pts[i - 1].position[0]) > o.width / 2)
          throw Error('Polar Transform inverse crosses the angle seam; split or rotate the source');
      }
    }
  }
  return copy;
}
