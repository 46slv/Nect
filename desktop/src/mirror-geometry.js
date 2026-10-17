// Geometry-only reflection. Keep source and reflected copies as independent groups
// so holes retain their winding relationship and overlapping fills do not cancel.
export function mirrorGeometry(source, { center, angle, keepOriginal, prefix }) {
  if (!source?.contours) throw Error('Connect a Path input');
  if (
    !Array.isArray(center) ||
    center.length !== 2 ||
    !center.every(Number.isFinite) ||
    !Number.isFinite(angle) ||
    typeof keepOriginal !== 'boolean'
  )
    throw Error('Mirror requires finite center and angle');
  const count = keepOriginal ? 2 : 1;
  const groups = new Map();
  for (const c of source.contours) {
    const key = c.group ?? '';
    if (!groups.has(key)) groups.set(key, groups.size);
  }
  if (
    source.contours.length * count > 10000 ||
    source.contours.reduce((sum, c) => sum + c.points.length, 0) * count > 100000 ||
    groups.size * count > 256
  )
    throw Error('Mirror exceeds geometry budget');
  const radians = ((angle % 180) * Math.PI) / 180;
  const a = Math.cos(2 * radians),
    b = Math.sin(2 * radians);
  const reflect = (v, handle = false) => {
    const x = v[0] - (handle ? 0 : center[0]),
      y = v[1] - (handle ? 0 : center[1]);
    const result = [
      a * x + b * y + (handle ? 0 : center[0]),
      b * x - a * y + (handle ? 0 : center[1]),
    ];
    if (!result.every(Number.isFinite)) throw Error('Mirror exceeds finite coordinates');
    return result;
  };
  const copies = keepOriginal ? ['original', 'reflected'] : ['reflected'];
  return {
    contours: copies.flatMap((copy) =>
      source.contours.map((c, ci) => ({
        id: `${prefix}:${copy}:c${ci}`,
        group: `${prefix}:${copy}:g${groups.get(c.group ?? '')}`,
        closed: c.closed,
        points: c.points.map((pt, pi) => ({
          id: `${prefix}:${copy}:c${ci}:p${pi}`,
          position: copy === 'original' ? [...pt.position] : reflect(pt.position),
          handleA: copy === 'original' ? [...pt.handleA] : reflect(pt.handleA, true),
          handleB: copy === 'original' ? [...pt.handleB] : reflect(pt.handleB, true),
        })),
      })),
    ),
  };
}
