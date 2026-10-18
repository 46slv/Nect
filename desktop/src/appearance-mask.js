import { paintedGroup, isPaintedGroup, appearanceExportBlockers } from './composition-reference.js';

function strokePadding(value, scale = 1) {
  if (isPaintedGroup(value)) {
    const [a, b, c, d] = value.matrix,
      next = scale * Math.hypot(a, b, c, d);
    return Math.max(0, ...value.children.map((child) => strokePadding(child, next)));
  }
  const stroke = value.stroke;
  return stroke
    ? ((Math.abs(stroke.width) * scale) / 2) *
        (stroke.join === 'miter' ? Math.max(1, stroke.miterLimit || 4) : 1)
    : 0;
}
export function maskAppearance(source, shape, { invert, prefix, bounds }) {
  if (!source || (!Array.isArray(source.contours) && source.kind !== 'image'))
    throw Error('Connect Image or Composition appearance to Mask');
  if (!Array.isArray(shape?.contours)) throw Error('Connect a closed Path shape to Mask');
  if (typeof invert !== 'boolean') throw Error('Mask invert must be Boolean');
  if (
    shape.contours.length > 256 ||
    shape.contours.reduce((n, c) => n + c.points.length, 0) > 10000
  )
    throw Error('Mask shape exceeds 256 contours or 10000 points');
  if (shape.contours.some((c) => !c.closed)) throw Error('Mask shape requires closed contours');
  const group = paintedGroup([source], { prefix }),
    pad = 2 + strokePadding(source),
    region = [
      bounds[0][0] - pad,
      bounds[1][0] - pad,
      Math.max(1, bounds[0][2] - bounds[0][0] + 2 * pad),
      Math.max(1, bounds[1][2] - bounds[1][0] + 2 * pad),
    ];
  if (!region.every((v) => Number.isFinite(v) && Math.abs(v) <= 1e9))
    throw Error('Mask paint bounds exceed the supported range');
  group.mask = { shape: { contours: structuredClone(shape.contours) }, invert, region };
  const blockers = appearanceExportBlockers([source, shape]);
  if (blockers.length) group.exportBlockers = blockers;
  return group;
}
