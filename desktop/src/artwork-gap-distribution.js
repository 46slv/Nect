import { cloneDocument, validate } from './core.js';
import { resolveArtworkDistribution } from './artwork-distribution.js';
import { artworkBoundsPrecision } from './artwork-alignment.js';
import { translateArtworkMoves } from './artwork-multi-move.js';
import { resolveArtworkMoveTarget } from './artwork-move-target.js';

export const artworkGapDistributions = [
  ['gap-x', '横方向', 0],
  ['gap-y', '縦方向', 1],
];

// Preserve the physical outside edges, even when the widest artwork is not
// last in leading-edge order. Exact ties use document order, never click order.
function orderedMembers(doc, members, axis) {
  const order = new Map(doc.layers.map((l, i) => [l.id, i]));
  const leading = members.toSorted(
    (a, b) =>
      a.bounds[axis][0] - b.bounds[axis][0] ||
      order.get(a.target.layerId) - order.get(b.target.layerId),
  );
  const first = leading[0];
  const last = members.toSorted(
    (a, b) =>
      b.bounds[axis][2] - a.bounds[axis][2] ||
      order.get(b.target.layerId) - order.get(a.target.layerId),
  )[0];
  if (first === last) throw Error('One artwork spans both outer edges; choose distinct endpoints.');
  return [first, ...leading.filter((m) => m !== first && m !== last), last];
}

// A Text outline can round each translated edge independently. With many
// members, summing their new widths can accumulate more than one outline step.
// Use the same adjacent-gap oracle before and after a move, so every accepted
// candidate is necessarily a no-op on the next click or after JSON reopening.
function equalGaps(bounds, precision) {
  const gaps = bounds.slice(1).map((b, i) => b[0] - bounds[i][2]);
  // Two independently rounded edges per gap, and two gaps at opposite error
  // extremes. This bound does not grow with the number of selected artworks.
  return gaps.every(Number.isFinite) && Math.max(...gaps) - Math.min(...gaps) <= 4 * precision;
}

export function distributeArtworkGaps(doc, layerIds, action, options = {}) {
  const distribution = artworkGapDistributions.find(([id]) => id === action);
  if (!distribution) throw Error('Choose horizontal or vertical artwork gap distribution.');
  const ids = [...layerIds];
  if (options.spacing !== undefined) {
    if (!Number.isFinite(options.spacing) || Math.abs(options.spacing) > 100000)
      throw Error('Spacing must be finite and within ±100000 px.');
    // Explicit spacing keeps the leading artwork fixed and moves every later
    // member. Auto mode below continues to preserve both outer endpoints.
    if (ids.length < 2 || ids.length > 100 || new Set(ids).size !== ids.length)
      throw Error('Select 2–100 unique artwork layers for fixed spacing.');
    const members = ids.map((id) => {
      const r = resolveArtworkMoveTarget(doc, id, options);
      if (!r.supported) throw Error(r.reason);
      return { target: r.target, bounds: r.bounds };
    });
    const axis = distribution[2],
      order = new Map(doc.layers.map((layer, i) => [layer.id, i]));
    members.sort(
      (a, b) =>
        a.bounds[axis][0] - b.bounds[axis][0] ||
        order.get(a.target.layerId) - order.get(b.target.layerId),
    );
    let end = members[0].bounds[axis][2];
    const moves = members.slice(1).map(({ target, bounds }) => {
      const start = end + options.spacing;
      end = start + bounds[axis][2] - bounds[axis][0];
      const delta = [0, 0];
      delta[axis] = start - bounds[axis][0];
      return { target, delta };
    });
    if (moves.every(({ delta }) => Math.abs(delta[axis]) <= artworkBoundsPrecision(members)))
      return false;
    const candidate = cloneDocument(doc);
    translateArtworkMoves(candidate, moves);
    validate(candidate);
    const next = members.map(({ target }) =>
      resolveArtworkMoveTarget(candidate, target.layerId, options),
    );
    if (next.some((member) => !member.supported))
      throw Error('Final artwork cannot retain fixed spacing.');
    if (
      next
        .slice(1)
        .some(
          (member, i) =>
            member.bounds[axis][0] < next[i].bounds[axis][0] - artworkBoundsPrecision(members),
        )
    )
      throw Error('Spacing would reverse artwork order.');
    if (
      next
        .slice(1)
        .some(
          (member, i) =>
            Math.abs(member.bounds[axis][0] - next[i].bounds[axis][2] - options.spacing) >
            artworkBoundsPrecision(members),
        )
    )
      throw Error('Final artwork cannot retain fixed spacing.');
    translateArtworkMoves(doc, moves);
    return true;
  }
  // Shared eligibility resolves every route afresh, including fixed endpoints.
  const resolved = resolveArtworkDistribution(doc, ids, options);
  if (!resolved.supported)
    throw Error(resolved.reason.replace('distribute centers', 'distribute gaps'));
  try {
    const axis = distribution[2];
    const members = orderedMembers(doc, resolved.members, axis);
    const precision = artworkBoundsPrecision(members);
    if (
      equalGaps(
        members.map((m) => m.bounds[axis]),
        precision,
      )
    )
      return false;
    const first = members[0].bounds[axis],
      last = members.at(-1).bounds[axis];
    const widths = members.map(({ bounds }) => bounds[axis][2] - bounds[axis][0]);
    const gap =
      (last[0] - first[2] - widths.slice(1, -1).reduce((sum, width) => sum + width, 0)) /
      (members.length - 1);
    if (!Number.isFinite(gap) || !widths.every(Number.isFinite))
      throw Error('Artwork gap calculation is not finite.');
    let end = first[2];
    const moves = members.slice(1, -1).map(({ target, bounds }, i) => {
      const start = end + gap;
      end = start + widths[i + 1];
      const delta = [0, 0];
      const distance = start - bounds[axis][0];
      delta[axis] = Math.abs(distance) <= 1e-9 ? 0 : distance;
      return { target, delta };
    });
    const candidate = cloneDocument(doc);
    translateArtworkMoves(candidate, moves);
    validate(candidate);
    const next = resolveArtworkDistribution(candidate, ids, options);
    if (!next.supported) throw Error(next.reason);
    // Negative gaps are valid overlap. Reject deep overlap that would change
    // endpoints/order on the next click, instead of resizing or oscillating.
    const nextMembers = orderedMembers(candidate, next.members, axis);
    if (nextMembers.some((m, i) => m.target.layerId !== members[i].target.layerId))
      throw Error(
        'Overlap would change artwork order or outer endpoints; spread them farther apart.',
      );
    const nextBounds = nextMembers.map((m) => m.bounds[axis]);
    if (nextBounds.slice(1).some((b, i) => b[0] < nextBounds[i][0] - precision))
      throw Error('Overlap would reverse artwork order; spread them farther apart.');
    if (
      nextBounds.some(([min, , max]) => min < first[0] - precision || max > last[2] + precision) ||
      !equalGaps(nextBounds, precision)
    )
      throw Error('Final artwork bounds cannot retain equal gaps and fixed outer edges.');
    translateArtworkMoves(doc, moves);
    return true;
  } catch (error) {
    throw Error(`${error.message} No selected artwork distributed.`);
  }
}
