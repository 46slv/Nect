// Transient ink-only geometry. Never writes typography, tracking, or character identities.
import paper from 'paper/dist/paper-core.js';
import { inverseCharacterDelta } from './character-geometry.js';
const scope = new paper.PaperScope();
scope.setup(new scope.Size(1, 1));

export function characterInkBounds(path, matrix) {
  if (!path || !Array.isArray(matrix) || matrix.length !== 6) return null;
  let item;
  try {
    inverseCharacterDelta(matrix, [0, 0]);
    item = new scope.CompoundPath({ pathData: path, insert: false });
    if (!item.children.some((child) => child.segments.length > 1)) return null;
    // Apply to the curves themselves, not the corners of a local bounding box.
    item.transform(new scope.Matrix(...matrix));
    const b = item.bounds,
      bounds = [b.left, b.top, b.right, b.bottom];
    return bounds.every(Number.isFinite) && b.width > 0 && b.height > 0 ? bounds : null;
  } catch {
    return null;
  } finally {
    item?.remove();
  }
}

export function collectCharacterSnapGeometry({
  characters,
  selectedIds,
  occurrence,
  lines = [],
  writingMode = 'horizontal-tb',
  boundsForOutline = characterInkBounds,
}) {
  const empty = { sources: [[], []], candidates: [], records: [] };
  if (!occurrence || occurrence.reason || occurrence.suspended) return empty;
  try {
    inverseCharacterDelta(occurrence.matrix, [0, 0]);
  } catch {
    return empty;
  }
  const selected = new Set(selectedIds),
    records = [];
  for (const glyph of characters) {
    if (!glyph.id || glyph.whitespace || !glyph.path) continue;
    const bounds = boundsForOutline(glyph.path, occurrence.matrix);
    if (!bounds || bounds.length !== 4 || !bounds.every(Number.isFinite)) continue;
    records.push({
      id: glyph.id,
      sourceIndex: glyph.sourceIndex,
      bounds,
      moving: selected.has(glyph.id),
      line: lines.findIndex(
        (line) => glyph.sourceIndex >= line.sourceStart && glyph.sourceIndex < line.sourceEnd,
      ),
    });
  }
  records.sort((a, b) => (a.id < b.id ? -1 : a.id > b.id ? 1 : 0));
  const sources = [[], []],
    candidates = [];
  for (const record of records)
    for (let axis = 0; axis < 2; axis++) {
      const low = record.bounds[axis],
        high = record.bounds[axis + 2];
      for (const [anchor, position] of [low, (low + high) / 2, high].entries()) {
        if (record.moving) sources[axis].push(position);
        else candidates.push({ axis, position, kind: 'character', targetId: record.id, anchor });
      }
    }
  const moving = records.filter((r) => r.moving);
  // Do not use incomplete selections (spaces, unknown mappings), multiple lines, or crossed runs.
  if (
    !moving.length ||
    moving.length !== selected.size ||
    moving[0].line < 0 ||
    moving.some((r) => r.line !== moving[0].line)
  )
    return { sources, candidates, records };
  const m = occurrence.matrix,
    vertical = writingMode === 'vertical-rl',
    vector = vertical ? [m[2], m[3]] : [m[0], m[1]],
    axis = Math.abs(vector[0]) >= Math.abs(vector[1]) ? 0 : 1;
  const run = records
    .filter((r) => r.line === moving[0].line)
    .sort(
      (a, b) =>
        a.bounds[axis] - b.bounds[axis] ||
        a.bounds[axis + 2] - b.bounds[axis + 2] ||
        (a.id < b.id ? -1 : 1),
    );
  let direction = 0;
  for (let i = 1; i < run.length; i++) {
    if (run[i - 1].bounds[axis + 2] > run[i].bounds[axis]) return { sources, candidates, records };
    const step = Math.sign(run[i].sourceIndex - run[i - 1].sourceIndex);
    if (!step || (direction && step !== direction)) return { sources, candidates, records };
    direction = step;
  }
  const first = run.findIndex((r) => r.moving),
    last = run.findLastIndex((r) => r.moving);
  if (run.slice(first, last + 1).some((r) => !r.moving)) return { sources, candidates, records };
  const lo = run[first].bounds[axis],
    hi = run[last].bounds[axis + 2],
    left = run[first - 1],
    right = run[last + 1];
  const add = (translation, segments, fixed, gap) => {
    if (
      !Number.isFinite(translation) ||
      gap < 0 ||
      (left && lo + translation < left.bounds[axis + 2]) ||
      (right && hi + translation > right.bounds[axis])
    )
      return;
    candidates.push({
      axis,
      translation,
      position: lo + translation,
      kind: 'character-gap',
      gap,
      segments,
      movingIds: moving.map((r) => r.id),
      targetIds: fixed.map((r) => r.id),
      fixedCross: Math.max(...fixed.map((r) => r.bounds[3 - axis])),
      movingCross: Math.max(...moving.map((r) => r.bounds[3 - axis])),
    });
  };
  if (left && right) {
    const d = (left.bounds[axis + 2] + right.bounds[axis] - lo - hi) / 2;
    add(
      d,
      [
        [left.bounds[axis + 2], lo + d],
        [hi + d, right.bounds[axis]],
      ],
      [left, right],
      lo + d - left.bounds[axis + 2],
    );
  }
  // At most the two adjacent stationary pairs touching this block; no all-pairs search.
  if (left && run[first - 2]) {
    const previous = run[first - 2],
      gap = left.bounds[axis] - previous.bounds[axis + 2],
      d = left.bounds[axis + 2] + gap - lo;
    add(
      d,
      [
        [previous.bounds[axis + 2], left.bounds[axis]],
        [left.bounds[axis + 2], lo + d],
      ],
      [previous, left],
      gap,
    );
  }
  if (right && run[last + 2]) {
    const next = run[last + 2],
      gap = next.bounds[axis] - right.bounds[axis + 2],
      d = right.bounds[axis] - gap - hi;
    add(
      d,
      [
        [hi + d, right.bounds[axis]],
        [right.bounds[axis + 2], next.bounds[axis]],
      ],
      [right, next],
      gap,
    );
  }
  return { sources, candidates, records };
}

// Paired short brackets in artboard coordinates. No export or authored geometry.
export function characterGapGuidePath(match, scale = 1) {
  const tick = 3 / scale,
    cross = match.cross + 8 / scale;
  const p = (along, across) => (match.axis ? `${across},${along}` : `${along},${across}`);
  return match.segments
    .map(
      ([a, b]) =>
        `M${p(a, cross)}L${p(b, cross)}M${p(a, cross - tick)}L${p(a, cross + tick)}M${p(b, cross - tick)}L${p(b, cross + tick)}`,
    )
    .join('');
}
