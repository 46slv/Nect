// Conservative curve-preserving local blends. No outline flattening is used.
const add = (a, b) => [a[0] + b[0], a[1] + b[1]];
const sub = (a, b) => [a[0] - b[0], a[1] - b[1]];
const mul = (a, s) => [a[0] * s, a[1] * s];
const dot = (a, b) => a[0] * b[0] + a[1] * b[1];
const cross = (a, b) => a[0] * b[1] - a[1] * b[0];
const norm = (a) => Math.hypot(...a);
const unit = (a) => mul(a, 1 / norm(a));
const eq = (a, b) => a[0] === b[0] && a[1] === b[1];
const lerp = (a, b, t) => add(mul(a, 1 - t), mul(b, t));
export function splitCubic(c, t) {
  const a = lerp(c[0], c[1], t),
    b = lerp(c[1], c[2], t),
    d = lerp(c[2], c[3], t);
  const e = lerp(a, b, t),
    f = lerp(b, d, t),
    p = lerp(e, f, t);
  return [
    [c[0], a, e, p],
    [p, f, d, c[3]],
  ];
}
export function trimCubic(c, a, b) {
  if (a === 0 && b === 1) return c.map((p) => [...p]);
  const left = b === 1 ? c : splitCubic(c, b)[0];
  return a === 0 ? left : splitCubic(left, a / b)[1];
}
const controls = (p, q) => [
  p.position,
  add(p.position, p.handleB),
  add(q.position, q.handleA),
  q.position,
];
const constant = (c) => c.every((p) => eq(p, c[0]));
const tangent = (c, end = false) => {
  const vectors = end
    ? [sub(c[3], c[2]), sub(c[3], c[1]), sub(c[3], c[0])]
    : [sub(c[1], c[0]), sub(c[2], c[0]), sub(c[3], c[0])];
  return unit(vectors.find((v) => norm(v) > 0) || [NaN, NaN]);
};
const polyLength = (c) => norm(sub(c[1], c[0])) + norm(sub(c[2], c[1])) + norm(sub(c[3], c[2]));
function lengthBounds(c, tol, budget, depth = 0) {
  if (--budget.length < 0) throw Error('precision-budget');
  const lo = norm(sub(c[3], c[0])),
    hi = polyLength(c);
  if (!Number.isFinite(hi)) throw Error('irregular');
  if (hi - lo <= tol) return [lo, hi];
  if (depth === 20) throw Error('precision-budget');
  const [a, b] = splitCubic(c, 0.5),
    x = lengthBounds(a, tol / 2, budget, depth + 1),
    y = lengthBounds(b, tol / 2, budget, depth + 1);
  return [x[0] + y[0], x[1] + y[1]];
}
function parameter(c, d, end, tol, budget) {
  let lo = 0,
    hi = 1;
  for (let i = 0; i < 48; i++) {
    const t = (lo + hi) / 2,
      piece = end ? trimCubic(c, t, 1) : trimCubic(c, 0, t);
    const bounds = lengthBounds(piece, tol / 4, budget);
    if (bounds[0] >= d - tol && bounds[1] <= d + tol) return t;
    if (bounds[1] < d) {
      if (end) hi = t;
      else lo = t;
    } else if (bounds[0] > d) {
      if (end) lo = t;
      else hi = t;
    } else return t; // bracket width is already <= tol/4
  }
  throw Error('precision-budget');
}
function hull(points) {
  const p = [...points].sort((a, b) => a[0] - b[0] || a[1] - b[1]);
  const half = (list) => {
    const h = [];
    for (const q of list) {
      while (h.length > 1 && cross(sub(h.at(-1), h.at(-2)), sub(q, h.at(-1))) <= 0) h.pop();
      h.push(q);
    }
    return h;
  };
  return [...half(p).slice(0, -1), ...half([...p].reverse()).slice(0, -1)];
}
const box = (c) =>
  c.reduce(
    (b, p) => [
      Math.min(b[0], p[0]),
      Math.min(b[1], p[1]),
      Math.max(b[2], p[0]),
      Math.max(b[3], p[1]),
    ],
    [Infinity, Infinity, -Infinity, -Infinity],
  );
const apart = (a, b) => a[2] < b[0] || b[2] < a[0] || a[3] < b[1] || b[3] < a[1];
// SAT permits exact contact, but never uses a positive epsilon to hide overlap.
function separated(a, b, contact = false) {
  for (const polygon of [a, b])
    for (let i = 0; i < polygon.length; i++) {
      const edge = sub(polygon[(i + 1) % polygon.length], polygon[i]),
        axis = [-edge[1], edge[0]];
      if (norm(axis) === 0) continue;
      const aa = a.map((p) => dot(sub(p, polygon[i]), axis)),
        bb = b.map((p) => dot(sub(p, polygon[i]), axis));
      const amax = Math.max(...aa),
        amin = Math.min(...aa),
        bmax = Math.max(...bb),
        bmin = Math.min(...bb);
      if (amax < bmin || bmax < amin) return true;
      if (contact && (amax === bmin || bmax === amin)) {
        const level = amax === bmin ? amax : bmax;
        const touchingB = b.filter((_, i) => bb[i] === level);
        // Only an intentional single endpoint may touch the replacement hull.
        if (
          touchingB.length &&
          touchingB.every((p) => eq(p, touchingB[0])) &&
          a.some((p) => eq(p, touchingB[0]))
        )
          return true;
      }
    }
  return false;
}
function clearRegion(region, c, tol, budget, depth = 0, contact = false) {
  if (--budget.collision < 0) throw Error('precision-budget');
  if (apart(box(region), box(c)) || separated(region, hull(c), contact)) return true;
  if (depth === 20 || polyLength(c) <= tol) return false;
  const [a, b] = splitCubic(c, 0.5);
  return (
    clearRegion(region, a, tol, budget, depth + 1, contact) &&
    clearRegion(region, b, tol, budget, depth + 1, contact)
  );
}
// A positive projection of every derivative control certifies regularity and
// injectivity throughout this piece, including limiting endpoint tangents.
function regular(c, u, tol) {
  const ds = [sub(c[1], c[0]), sub(c[2], c[1]), sub(c[3], c[2])];
  return ds.every((d) => dot(d, u) >= 0) && ds.some((d) => dot(d, u) > tol);
}
export const hasCurves = (contour) =>
  contour.points.some((p) => [...p.handleA, ...p.handleB].some((v) => v !== 0));
export function roundMixedContours(source, { radius, prefix, addPoint, legacy }) {
  const budget = { length: 200000, collision: 200000 };
  const diagnostics = { rounded: 0, smooth: 0, skipped: {} };
  const skip = (reason) => (diagnostics.skipped[reason] = (diagnostics.skipped[reason] || 0) + 1);
  const records = source.contours.map((contour, ci) => {
    const points = contour.points,
      segments = [];
    for (let i = 0; i < (contour.closed ? points.length : points.length - 1); i++) {
      const j = (i + 1) % points.length,
        c = controls(points[i], points[j]);
      segments.push({ ci, i, j, c, constant: constant(c) });
    }
    return {
      contour,
      segments,
      active: segments.filter((s) => !s.constant),
      proposals: [],
      legacy: hasCurves(contour) ? null : legacy(contour, ci),
    };
  });
  const obstacles = records.flatMap((r) => {
    if (!r.legacy) return r.segments;
    const ps = r.legacy.points,
      ss = [];
    for (let i = 0; i < (r.legacy.closed ? ps.length : ps.length - 1); i++) {
      const c = controls(ps[i], ps[(i + 1) % ps.length]);
      ss.push({ c, constant: constant(c) });
    }
    return ss;
  });
  // Uniform broad phase is bounded: excessively broad queries are rejected.
  const allBox = box(obstacles.flatMap((s) => [s.c[0], s.c[3]]));
  const cell = Math.max(
    1,
    Math.max(allBox[2] - allBox[0], allBox[3] - allBox[1]) /
      Math.max(1, Math.sqrt(obstacles.length)),
  );
  const grid = new Map(),
    overflow = [];
  let indexEntries = 0;
  const cells = (b) => {
    const x0 = Math.floor(b[0] / cell),
      x1 = Math.floor(b[2] / cell),
      y0 = Math.floor(b[1] / cell),
      y1 = Math.floor(b[3] / cell);
    if (![x0, x1, y0, y1].every(Number.isSafeInteger) || (x1 - x0 + 1) * (y1 - y0 + 1) > 4096)
      return null;
    const out = [];
    for (let x = x0; x <= x1; x++) for (let y = y0; y <= y1; y++) out.push(`${x},${y}`);
    return out;
  };
  for (const s of obstacles) {
    const keys = cells(box(s.c));
    if (!keys || indexEntries + keys.length > 200000) {
      overflow.push(s);
      continue;
    }
    indexEntries += keys.length;
    for (const k of keys) {
      if (!grid.has(k)) grid.set(k, []);
      grid.get(k).push(s);
    }
  }
  const accepted = [];
  for (const r of records) {
    if (!hasCurves(r.contour)) continue;
    if (r.active.length < 2) {
      skip('irregular');
      continue;
    }
    for (let k = r.contour.closed ? 0 : 1; k < r.active.length; k++) {
      const incoming = r.active[(k - 1 + r.active.length) % r.active.length],
        outgoing = r.active[k];
      const connectors = new Set();
      for (
        let i = (incoming.i + 1) % r.segments.length;
        i !== outgoing.i;
        i = (i + 1) % r.segments.length
      ) {
        if (connectors.size >= r.segments.length) break;
        connectors.add(r.segments[i]);
      }
      const u = tangent(incoming.c, true),
        v = tangent(outgoing.c),
        turn = Math.atan2(cross(u, v), dot(u, v));
      // DejaVu Serif O has an intentional near-smooth 0.004487 rad join.
      // Preserve turns below 0.005 rad (0.287 degrees) as a font precision policy.
      if (Math.abs(turn) < 0.005) {
        diagnostics.smooth++;
        continue;
      }
      if (!Number.isFinite(turn) || Math.PI - Math.abs(turn) < 1e-4) {
        skip('irregular');
        continue;
      }
      const scale = Math.max(polyLength(incoming.c), polyLength(outgoing.c));
      const coord = Math.max(
        ...incoming.c.flat().map(Math.abs),
        ...outgoing.c.flat().map(Math.abs),
      );
      const tol = Math.max(1e-7, scale * 1e-7, 32 * Number.EPSILON * coord);
      if (budget.length <= 0 || budget.collision <= 0) {
        skip('precision-budget');
        continue;
      }
      let reason = 'irregular',
        proposal = null;
      try {
        const li = lengthBounds(incoming.c, tol, budget)[0],
          lo = lengthBounds(outgoing.c, tol, budget)[0];
        let d = Math.min(
          radius * Math.tan(Math.abs(turn) / 2),
          (li - 4 * tol) / 2,
          (lo - 4 * tol) / 2,
        );
        for (let retry = 0; retry < 12 && d > 8 * tol; retry++, d /= 2) {
          const ti = parameter(incoming.c, d, true, tol, budget),
            to = parameter(outgoing.c, d, false, tol, budget);
          const left = trimCubic(incoming.c, 0, ti),
            right = trimCubic(outgoing.c, to, 1),
            removedI = trimCubic(incoming.c, ti, 1),
            removedO = trimCubic(outgoing.c, 0, to);
          const a = left[3],
            b = right[0],
            uu = tangent(left, true),
            vv = tangent(right),
            det = cross(uu, vv),
            phi = Math.atan2(det, dot(uu, vv));
          if (
            !regular(removedI, u, tol) ||
            !regular(removedO, v, tol) ||
            Math.abs(det) < Math.sin(1e-4) ||
            phi * turn <= 0 ||
            Math.abs(phi - turn) > 0.5
          )
            continue;
          const aa = cross(sub(b, a), vv) / det,
            bb = cross(uu, sub(b, a)) / det;
          if (!(aa > tol && bb > tol && aa <= 4 * d && bb <= 4 * d)) continue;
          const h = ((4 / 3) * Math.cos(phi / 2)) / (1 + Math.cos(phi / 2));
          const bridge = [a, add(a, mul(uu, h * aa)), sub(b, mul(vv, h * bb)), b];
          if (!bridge.flat().every(Number.isFinite)) continue;
          const region = hull([...removedI, ...removedO, ...bridge]);
          // The two removed pieces must lie in opposite vertex half-planes.
          const bisector = add(u, v),
            vertex = incoming.c[3];
          if (
            !removedI.every((p) => dot(sub(p, vertex), bisector) <= 0) ||
            !removedO.every((p) => dot(sub(p, vertex), bisector) >= 0)
          )
            continue;
          reason = 'collision-ambiguity';
          if (
            !clearRegion(region, left, tol, budget, 0, true) ||
            !clearRegion(region, right, tol, budget, 0, true)
          )
            continue;
          const keys = cells(box(region));
          if (!keys) throw Error('precision-budget');
          const near = new Set(overflow);
          for (const key of keys) for (const s of grid.get(key) || []) near.add(s);
          let safe = true;
          for (const s of near) {
            if (--budget.collision < 0) throw Error('precision-budget');
            if (s === incoming || s === outgoing) continue;
            // Constant connectors at this exact vertex are logically transparent.
            if (connectors.has(s)) continue;
            if (!clearRegion(region, s.c, tol, budget)) {
              safe = false;
              break;
            }
          }
          if (!safe) continue;
          for (const other of accepted) {
            if (--budget.collision < 0) throw Error('precision-budget');
            if (!apart(box(region), box(other.region)) && !separated(region, other.region)) {
              safe = false;
              break;
            }
          }
          if (!safe) continue;
          proposal = { incoming, outgoing, ti, to, bridge, region, connectors };
          break;
        }
      } catch (error) {
        reason = error.message === 'precision-budget' ? 'precision-budget' : 'irregular';
      }
      if (proposal) {
        r.proposals.push(proposal);
        accepted.push(proposal);
        diagnostics.rounded++;
      } else skip(reason);
    }
  }
  const contours = records.map((r, ci) => {
    if (!hasCurves(r.contour)) return r.legacy;
    const copy = (p) => ({
      ...p,
      position: [...p.position],
      handleA: [...p.handleA],
      handleB: [...p.handleB],
    });
    if (!r.proposals.length)
      return {
        ...r.contour,
        id: `${prefix}:c${ci}`,
        points: r.contour.points.map((p, i) => {
          addPoint();
          return { ...copy(p), id: `${prefix}:c${ci}:p${i}` };
        }),
      };
    const starts = new Map(),
      ends = new Map();
    for (const p of r.proposals) {
      ends.set(p.incoming, p);
      starts.set(p.outgoing, p);
    }
    const pieces = [],
      removedConnectors = new Set(r.proposals.flatMap((p) => [...p.connectors]));
    for (const s of r.segments) {
      const start = starts.get(s),
        end = ends.get(s);
      // Remove only constant records in an edited logical junction.
      if (removedConnectors.has(s)) continue;
      pieces.push({
        c: trimCubic(s.c, start?.to || 0, end?.ti ?? 1),
        from: r.contour.points[s.i],
        to: r.contour.points[s.j],
        untouched: !start && !end,
      });
      if (end) pieces.push({ c: end.bridge });
    }
    const out = [];
    for (const piece of pieces) {
      const c = piece.c;
      let p = out.at(-1);
      if (!p) {
        p = copy(piece.from || { position: c[0], handleA: [0, 0], handleB: [0, 0] });
        p.position = [...c[0]];
        out.push(p);
      }
      p.handleB = piece.untouched ? [...piece.from.handleB] : sub(c[1], c[0]);
      out.push({
        ...copy(piece.to || { position: c[3], handleA: [0, 0], handleB: [0, 0] }),
        position: [...c[3]],
        handleA: piece.untouched ? [...piece.to.handleA] : sub(c[2], c[3]),
      });
    }
    if (r.contour.closed && out.length > 1) {
      out[0].handleA = out.at(-1).handleA;
      out.pop();
    }
    return {
      ...r.contour,
      id: `${prefix}:c${ci}`,
      points: out.map((p, i) => {
        addPoint();
        return { ...p, id: `${prefix}:c${ci}:p${i}` };
      }),
    };
  });
  return { contours, cornerDiagnostics: diagnostics };
}
