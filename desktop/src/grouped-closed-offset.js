import {
  offsetClosedPathDetailed,
  OFFSET_EDITOR_LIMITS,
  OFFSET_LIMITS,
  OffsetError,
} from './closed-offset.js';
import { fingerprint } from './fingerprint.js';

const aggregateLimits = [
  { diagnostic: 'flattenedVertices', limit: 'flattenedVertices' },
  { diagnostic: 'subdivisionNodes', limit: 'subdivisionNodes' },
  { diagnostic: 'topologyPairChecks', limit: 'pairChecks' },
  { diagnostic: 'estimatedJoinVertices', limit: 'estimatedJoinVertices' },
  { diagnostic: 'outputVertices', limit: 'outputVertices' },
];

function effectiveLimits(workLimits) {
  if (!workLimits || typeof workLimits !== 'object' || Array.isArray(workLimits))
    throw new OffsetError('OFFSET_OPTIONS', 'Offset work limits must be an object');
  for (const [key, value] of Object.entries(workLimits))
    if (
      !Object.hasOwn(OFFSET_EDITOR_LIMITS, key) ||
      !Number.isInteger(value) ||
      value <= 0 ||
      value > OFFSET_LIMITS[key]
    )
      throw new OffsetError(
        'OFFSET_OPTIONS',
        'Offset work limits may only tighten qualified budgets',
      );
  return { ...OFFSET_EDITOR_LIMITS, ...workLimits };
}

function groupedContours(path) {
  const groups = new Map();
  for (const contour of path.contours) {
    const key = contour?.group ?? '';
    if (!groups.has(key)) groups.set(key, []);
    groups.get(key).push(contour);
  }
  return [...groups.values()];
}

function consumed(diagnostics, key) {
  const value = diagnostics?.[key];
  return value === undefined ? 0 : value;
}

/**
 * Offset each explicit contour group as its own nonzero region collection.
 * Ungrouped paths intentionally remain on the existing one-call kernel path.
 */
export function offsetGroupedClosedPathDetailed(
  path,
  amount,
  options = {},
  workLimits = OFFSET_EDITOR_LIMITS,
) {
  if (!Number.isFinite(amount))
    throw new OffsetError('OFFSET_AMOUNT', 'Offset amount must be finite');
  if (amount === 0) return offsetClosedPathDetailed(path, amount, options, workLimits);
  if (!path || !Array.isArray(path.contours))
    throw new OffsetError('OFFSET_GEOMETRY', 'Offset requires a Nect-SVG Path');
  if (!path.contours.some((contour) => contour?.group !== undefined))
    return offsetClosedPathDetailed(path, amount, options, workLimits);

  if (path.kind !== undefined)
    throw new OffsetError('OFFSET_GEOMETRY', 'Offset requires a Nect-SVG Path');
  if (!Array.from(path.contours).every((contour) => contour && typeof contour === 'object'))
    throw new OffsetError(
      'OFFSET_GEOMETRY',
      'Offset contours must be a dense array of contour objects',
    );
  if (
    !path.contours.every(
      (contour) => contour.group === undefined || typeof contour.group === 'string',
    )
  )
    throw new OffsetError('OFFSET_GEOMETRY', 'Offset contour groups must be strings');

  if (!options || typeof options !== 'object' || Array.isArray(options))
    throw new OffsetError('OFFSET_OPTIONS', 'Offset options must be an object');
  const sourcePrefix = options.prefix === undefined ? 'offset' : options.prefix;
  if (typeof sourcePrefix !== 'string' || sourcePrefix.length === 0 || sourcePrefix.length > 256)
    throw new OffsetError(
      'OFFSET_OPTIONS',
      'Offset output prefix must be a nonempty string of at most 256 characters',
    );

  const limits = effectiveLimits(workLimits);
  if (path.contours.length > limits.contours)
    throw new OffsetError('OFFSET_LIMIT', `Offset compound exceeds ${limits.contours} contours`);
  const sourcePoints = path.contours.reduce(
    (count, contour) => count + (Array.isArray(contour?.points) ? contour.points.length : 0),
    0,
  );
  if (sourcePoints > limits.sourcePoints)
    throw new OffsetError('OFFSET_LIMIT', `Offset source exceeds ${limits.sourcePoints} points`);

  const groups = groupedContours(path);
  if (groups.length > limits.contours)
    throw new OffsetError('OFFSET_LIMIT', `Offset grouped input exceeds ${limits.contours} groups`);

  const remaining = Object.fromEntries(aggregateLimits.map(({ limit }) => [limit, limits[limit]]));
  const diagnostics = Object.fromEntries(aggregateLimits.map(({ diagnostic }) => [diagnostic, 0]));
  const contours = [];
  const basePrefix = sourcePrefix;

  for (const [groupIndex, group] of groups.entries()) {
    if (groupIndex > 0 && aggregateLimits.some(({ limit }) => remaining[limit] <= 0))
      throw new OffsetError(
        'OFFSET_LIMIT',
        'Offset grouped work budget is exhausted before the next group',
      );
    const suffix = `:g${groupIndex}`;
    const prefix =
      basePrefix.length + suffix.length <= 256
        ? `${basePrefix}${suffix}`
        : `offset-${fingerprint(basePrefix)}${suffix}`;
    const callLimits = {
      ...limits,
      ...Object.fromEntries(aggregateLimits.map(({ limit }) => [limit, remaining[limit]])),
    };
    const region = {
      contours: group.map(({ group: _group, ...contour }) => contour),
    };
    const result = offsetClosedPathDetailed(region, amount, { ...options, prefix }, callLimits);

    for (const { diagnostic, limit } of aggregateLimits) {
      const used = consumed(result.diagnostics, diagnostic);
      if (!Number.isInteger(used) || used < 0 || used > remaining[limit])
        throw new OffsetError('OFFSET_LIMIT', `Offset grouped ${diagnostic} work limit exceeded`);
      remaining[limit] -= used;
      diagnostics[diagnostic] += used;
    }

    const groupTag = prefix;
    for (const contour of result.path.contours) contours.push({ ...contour, group: groupTag });
  }

  return {
    path: { contours },
    diagnostics: { exactPassthrough: false, groups: groups.length, ...diagnostics },
  };
}
