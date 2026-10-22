import { snapGraphNodes } from './layout.js';

// Gesture-only constraints in displayed artboard coordinates. Convert to source
// coordinates after this step, including for rotated or reflected parents.
const project = (delta, direction) => {
  const t =
    (delta[0] * direction[0] + delta[1] * direction[1]) / (direction[0] ** 2 + direction[1] ** 2);
  return direction.map((v) => v * t);
};
export function constrainDirectionDrag(state, delta, screenDelta, shift) {
  if (!shift) {
    state.direction = null;
    state.axis = null;
    return { delta: [...delta], direction: null, axis: null, pending: false };
  }
  if (!state.direction) {
    if (Math.hypot(...screenDelta) < 3)
      return { delta: [0, 0], direction: null, axis: null, pending: true };
    const x = Math.abs(delta[0]),
      y = Math.abs(delta[1]);
    // Nearest unoriented 45 degree line; ties at 22.5 degrees choose an axis.
    state.direction =
      Math.min(x, y) > Math.max(x, y) * Math.tan(Math.PI / 8)
        ? [1, delta[0] * delta[1] < 0 ? -1 : 1]
        : x >= y
          ? [1, 0]
          : [0, 1];
  }
  const direction = state.direction;
  const axis = direction[0] === 0 ? 1 : direction[1] === 0 ? 0 : null;
  state.axis = axis;
  return { delta: project(delta, direction), direction, axis, pending: false };
}

const priority = {
  guide: 0,
  character: 1,
  'character-gap': 2,
  'guide-layer': 3,
  artboard: 4,
  artwork: 5,
  grid: 6,
};
const same = (a, b) => Math.abs(a - b) < 1e-8;
const directionGuide = (origin, delta) => ({
  kind: 'direction',
  start: [...origin],
  end: origin.map((v, i) => v + delta[i]),
});

// Reuse the existing target search/priority. A diagonal has only one degree of
// freedom: one winning axis target determines both coordinates. Never snap axes
// independently and then project, which would display a guide the point misses.
export function snapDirectionTranslation(
  snap,
  sources,
  delta,
  {
    direction = null,
    scale = 1,
    enabled = true,
    origin = [0, 0],
    offset = [0, 0],
    automatic = true,
  } = {},
) {
  const safeScale = Math.max(0.0001, Number.isFinite(scale) ? Math.abs(scale) : 1);
  if (direction?.every((v) => v !== 0)) {
    const base = project(
      delta.map((v, i) => v + offset[i]),
      direction,
    ).map((v, i) => v - offset[i]);
    const found = snap(sources, base, safeScale * Math.SQRT2, {
      direction,
      gridSources: sources,
      gridScale: safeScale,
    });
    // The rotated grid resolves its line/ray intersection in world coordinates.
    // Do not reinterpret a grid-local axis as a world-axis target.
    if (found.matches.some((match) => match.gridLine)) {
      const matches = [...found.matches];
      if (enabled)
        matches.push(
          directionGuide(
            origin,
            found.delta.map((v, i) => v + offset[i]),
          ),
        );
      return { delta: found.delta, matches };
    }
    const choices = found.matches
      .map((match) => ({
        match,
        correction: found.delta[match.axis] - base[match.axis],
      }))
      .sort(
        (a, b) =>
          // As in the original search, explicit targets precede the background grid.
          Number(a.match.kind === 'grid') - Number(b.match.kind === 'grid') ||
          Math.abs(a.correction) - Math.abs(b.correction) ||
          (priority[a.match.kind] ?? 0) - (priority[b.match.kind] ?? 0) ||
          a.match.axis - b.match.axis,
      );
    const winner = choices[0];
    const result = winner
      ? base.map(
          (v, axis) => v + (winner.correction * direction[axis]) / direction[winner.match.axis],
        )
      : base;
    const matches = choices
      .filter(({ match, correction }) => same(result[match.axis], base[match.axis] + correction))
      .map(({ match }) => ({ ...match }));
    for (const match of matches)
      if (match.kind === 'character-gap')
        match.cross = Math.max(match.fixedCross, match.movingCross + result[1 - match.axis]);
    if (enabled)
      matches.push(
        directionGuide(
          origin,
          result.map((v, i) => v + offset[i]),
        ),
      );
    return { delta: result, matches };
  }
  const allowed = direction
    ? sources.map((values, axis) => (direction[axis] ? values : []))
    : sources;
  const found = snap(allowed, delta, safeScale, {
    direction,
    gridSources: sources,
    gridScale: safeScale,
  });
  // Ordinary explicit targets retain their exact behavior. When no target wins,
  // Snap also attracts to a 45 degree ray through the gesture origin (6 CSS px).
  if (
    !direction &&
    automatic &&
    enabled &&
    !found.matches.length &&
    sources.every((values) => values.length) &&
    Math.hypot(...delta) * safeScale >= 3
  ) {
    const vector = delta.map((v, i) => v + offset[i]);
    if (
      Math.min(...vector.map(Math.abs)) <=
      Math.max(...vector.map(Math.abs)) * Math.tan(Math.PI / 8)
    )
      return found;
    const diagonal = [1, vector[0] * vector[1] < 0 ? -1 : 1];
    const projected = project(vector, diagonal).map((v, i) => v - offset[i]);
    if (Math.hypot(...delta.map((v, i) => v - projected[i])) * safeScale <= 6)
      return {
        delta: projected,
        matches: [
          directionGuide(
            origin,
            projected.map((v, i) => v + offset[i]),
          ),
        ],
      };
  }
  return found;
}

export function directionGuidePath(match) {
  return `M${match.start[0]} ${match.start[1]}L${match.end[0]} ${match.end[1]}`;
}

export function snapGraphDirection(
  moving,
  targets,
  delta,
  zoom,
  bypass = false,
  threshold = 6,
  { state = {}, shift = false, screenDelta = delta.map((v) => v * zoom) } = {},
) {
  if (!moving.length) return { delta: [...delta], guides: [] };
  const constraint = constrainDirectionDrag(state, delta, screenDelta, shift);
  if (constraint.pending) return { delta: [0, 0], guides: [] };
  const origin = [Math.min(...moving.map((b) => b.x)), Math.min(...moving.map((b) => b.y))];
  const snap = (sources, value, scale) => {
    const found = snapGraphNodes(moving, targets, value, scale, bypass, threshold);
    // Axis constraints must suppress target searches on the forbidden axis.
    return {
      delta: found.delta.map((v, axis) => (sources[axis].length ? v : value[axis])),
      matches: found.guides
        .filter((g) => sources[g.axis].length)
        .map((g) => ({ ...g, kind: 'graph' })),
    };
  };
  const result = snapDirectionTranslation(snap, [[0], [0]], constraint.delta, {
    direction: constraint.direction,
    scale: zoom,
    enabled: !bypass,
    origin,
  });
  let value = result.delta;
  const guide = result.matches.find((m) => m.kind === 'direction');
  if (!constraint.direction && !guide)
    return { delta: value, guides: result.matches.map(({ kind, ...guide }) => guide) };
  if (constraint.direction?.every((v) => v !== 0) || guide) {
    const direction = constraint.direction || [1, value[0] * value[1] < 0 ? -1 : 1];
    let t = value[0] / direction[0];
    let low = -Infinity,
      high = Infinity;
    for (let axis = 0; axis < 2; axis++) {
      const bound = -origin[axis] / direction[axis];
      if (direction[axis] > 0) low = Math.max(low, bound);
      else high = Math.min(high, bound);
    }
    t = Math.min(high, Math.max(low, t));
    value = direction.map((v) => v * t);
  } else value = value.map((v, axis) => Math.max(v, -origin[axis]));
  // Recompute finite guide extents against the actual constrained final delta.
  const exact = snapGraphNodes(moving, targets, value, zoom, bypass, 1e-7);
  return {
    delta: value,
    guides: [...exact.guides, ...(guide ? [directionGuide(origin, value)] : [])],
  };
}
