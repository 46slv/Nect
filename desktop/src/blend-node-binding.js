import { prepareClosedContourBlend } from './blend-geometry.js';
import { colorOperation, publicColor } from './color-operations.js';

// First vertical: one solid-filled closed contour per source. Complex painted
// groups and incompatible strokes must not silently lose their appearance.
export function evaluateBlend(_operation, read, context) {
  const a = read('a'),
    b = read('b'),
    steps = read('steps');
  if (!a || !b) throw Error('Connect both Blend inputs');
  if (!Number.isInteger(steps) || steps < 0 || steps > 254)
    throw Error('Blend steps must be an integer from 0 to 254');
  for (const value of [a, b]) {
    if (value.kind || value.mask || value.contours?.length !== 1 || !value.fill)
      throw Error('Blend currently supports single solid-filled contours without mask');
    if (
      [value, value.contours[0]].some((item) =>
        Object.keys(item).some((key) => key.startsWith('repeater')),
      ) ||
      value.characterOccurrences?.length
    )
      throw Error('Blend does not support per-copy appearance');
    publicColor(colorOperation('mix', [value.fill, value.fill, 0]));
  }
  if (!!a.stroke !== !!b.stroke) throw Error('Blend requires matching stroke presence');
  let strokeA, strokeB, strokeStyle;
  if (a.stroke) {
    const { hex: ah, alpha: aa, width: aw, ...as } = a.stroke;
    const { hex: bh, alpha: ba, width: bw, ...bs } = b.stroke;
    if (
      !Number.isFinite(aw) ||
      aw < 0 ||
      !Number.isFinite(bw) ||
      bw < 0 ||
      JSON.stringify(as) !== JSON.stringify(bs)
    )
      throw Error('Blend requires compatible solid strokes');
    strokeA = { hex: ah, alpha: aa };
    strokeB = { hex: bh, alpha: ba };
    publicColor(colorOperation('mix', [strokeA, strokeB, 0]));
    strokeStyle = as;
  }
  if ((steps + 2) * a.contours[0].points.length > 100000)
    throw Error('Blend exceeds geometry budget');
  const blockers = [...new Set([...(a.exportBlockers || []), ...(b.exportBlockers || [])])];
  const interpolate = prepareClosedContourBlend(a.contours[0], b.contours[0]);
  const values = [];
  for (let i = 0; i < steps + 2; i++) {
    const t = i / (steps + 1);
    values.push({
      contours: [interpolate(t, `${context.nodeId}:blend:${i}`)],
      fill: publicColor(colorOperation('mix', [a.fill, b.fill, t])),
      ...(a.stroke
        ? {
            stroke: {
              ...publicColor(colorOperation('mix', [strokeA, strokeB, t])),
              width: a.stroke.width + (b.stroke.width - a.stroke.width) * t,
              ...strokeStyle,
            },
          }
        : {}),
      ...(blockers.length ? { exportBlockers: blockers } : {}),
    });
  }
  return context.groupAppearance(values);
}
