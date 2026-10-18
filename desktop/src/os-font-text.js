import { normalizeOsText, osOutlineGeometry } from './os-font-outline.js';

const ownKeys = (value, keys) =>
  value &&
  Object.getPrototypeOf(value) === Object.prototype &&
  Object.keys(value).length === keys.length &&
  Object.keys(value).every((key) => keys.includes(key));

// The snapshot is authored geometry, never font software or an evaluation cache.
export function validateOsFontText(n, schemaVersion = 28) {
  const ref = n.osFont;
  if (ref === undefined) return;
  if (
    schemaVersion < 28 ||
    n.type !== 'Text' ||
    n.font !== 'default' ||
    !ownKeys(ref, ['version', 'family', 'style', 'outline']) ||
    ref.version !== 1 ||
    typeof ref.family !== 'string' ||
    !ref.family.trim() ||
    ref.family.length > 256 ||
    /[\u0000-\u001f]/.test(ref.family) ||
    !Number.isInteger(ref.style) ||
    ref.style < 0 ||
    ref.style > 3 ||
    !ownKeys(ref.outline, ['family', 'style', 'text', 'size', 'ascent', 'points', 'types']) ||
    ref.outline.family !== ref.family ||
    ref.outline.style !== ref.style ||
    ref.outline.text !== normalizeOsText(n.text, { allowEmpty: true })
  )
    throw Error('Invalid OS font Text snapshot or schema');
  if (
    !Number.isFinite(n.params.size.value) ||
    n.params.size.value < 0.01 ||
    n.params.size.value > 10000
  )
    throw Error('OS font Text size must be 0.01–10000 px');
  for (const key of ['letterSpacing', 'lineSpacing']) {
    const q = n.params[key];
    if (q.source || q.expression) throw Error('OS font Text spacing cannot be driven');
  }
  // Reuse the exact outline validator, but do not make display dependent on locks
  // or a native font. Position/Size bindings are resolved by the common evaluator.
  const shadow = {
    ...n,
    locked: false,
    bypassed: false,
    params: Object.fromEntries(
      Object.entries(n.params).map(([key, q]) => [
        key,
        {
          ...q,
          locked: false,
          source: undefined,
          sourcePort: undefined,
          expression: '',
          value: key === 'size' ? ref.outline.size : q.value,
        },
      ]),
    ),
  };
  let id = 0;
  return osOutlineGeometry(ref.outline, shadow, (prefix) => `${n.id}:os:${prefix}${id++}`, {
    allowEmpty: true,
  });
}

export function osFontTextPath(n, position, size, boundsOf) {
  const path = validateOsFontText(n),
    ratio = size / n.osFont.outline.size;
  if (!Number.isFinite(size) || size < 0.01 || size > 10000 || !position?.every(Number.isFinite))
    throw Error('OS font Text evaluated Position/Size is outside its supported range');
  for (const c of path.contours)
    for (const pt of c.points)
      for (const key of ['position', 'handleA', 'handleB']) pt[key] = pt[key].map((v) => v * ratio);
  const bounds = boundsOf(path),
    x = bounds[0][n.alignment === 'center' ? 1 : n.alignment === 'end' ? 2 : 0],
    y =
      n.crossAlignment && n.crossAlignment !== 'origin'
        ? bounds[1][n.crossAlignment === 'center' ? 1 : n.crossAlignment === 'end' ? 2 : 0]
        : n.osFont.outline.ascent * ratio;
  const delta = [position[0] - x, position[1] - y];
  for (const c of path.contours)
    for (const pt of c.points) pt.position = pt.position.map((v, k) => v + delta[k]);
  return path;
}
