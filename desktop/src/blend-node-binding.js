import { blendPointCount, prepareClosedContourBlend } from './blend-geometry.js';
import { colorOperation, publicColor } from './color-operations.js';

// Mixing a fully transparent color erases its RGB. Endpoints retain authored paint.
const blendColor = (a, b, t) =>
  t === 0 ? { ...a } : t === 1 ? { ...b } : publicColor(colorOperation('mix', [a, b, t]));

// First vertical: one closed contour with matching solid fill/stroke presence. Complex painted
// groups and incompatible strokes must not silently lose their appearance.
export function evaluateBlend(_operation, read, context) {
  const a = read('a'),
    b = read('b'),
    steps = read('steps');
  if (!a || !b) throw Error('Connect both Blend inputs');
  if (!Number.isInteger(steps) || steps < 0 || steps > 254)
    throw Error('Blend steps must be an integer from 0 to 254');
  for (const value of [a, b]) {
    if (value.kind || value.mask || value.contours?.length !== 1)
      throw Error('Blend currently supports single closed contours without mask');
    if (
      [value, value.contours[0]].some((item) =>
        Object.keys(item).some((key) => key.startsWith('repeater')),
      ) ||
      value.characterOccurrences?.length
    )
      throw Error('Blend does not support per-copy appearance');
    if (value.fill != null) publicColor(colorOperation('mix', [value.fill, value.fill, 0]));
    if (value.fill == null && !value.stroke) throw Error('Blend requires visible paint slots');
  }
  if ((a.fill != null) !== (b.fill != null)) throw Error('Blend requires matching fill presence');
  if (!!a.stroke !== !!b.stroke) throw Error('Blend requires matching stroke presence');
  let strokeA, strokeB, strokeStyle;
  if (a.stroke) {
    const { hex: ah, alpha: aa, width: aw, ...as } = a.stroke;
    const { hex: bh, alpha: ba, width: bw, ...bs } = b.stroke;
    if (
      !Number.isFinite(aw) ||
      aw < 0 ||
      aw > 100000 ||
      !Number.isFinite(bw) ||
      bw < 0 ||
      bw > 100000 ||
      [as, bs].some(
        (style) =>
          (style.kind !== undefined && style.kind !== 'solid') ||
          (style.dash && (!Array.isArray(style.dash) || style.dash.length)),
      ) ||
      JSON.stringify(
        Object.keys(as)
          .sort()
          .map((key) => [key, as[key]]),
      ) !==
        JSON.stringify(
          Object.keys(bs)
            .sort()
            .map((key) => [key, bs[key]]),
        )
    )
      throw Error('Blend requires compatible solid strokes');
    strokeA = { hex: ah, alpha: aa };
    strokeB = { hex: bh, alpha: ba };
    publicColor(colorOperation('mix', [strokeA, strokeB, 0]));
    strokeStyle = as;
  }
  if (
    (steps + 2) * blendPointCount(a.contours[0].points.length, b.contours[0].points.length) >
    100000
  )
    throw Error('Blend exceeds geometry budget');
  const blockers = [...new Set([...(a.exportBlockers || []), ...(b.exportBlockers || [])])];
  const interpolate = prepareClosedContourBlend(a.contours[0], b.contours[0]);
  const values = [];
  for (let i = 0; i < steps + 2; i++) {
    const t = i / (steps + 1);
    values.push({
      contours: [interpolate(t, `${context.nodeId}:blend:${i}`)],
      fill: a.fill == null ? null : blendColor(a.fill, b.fill, t),
      ...(a.stroke
        ? {
            stroke: {
              ...blendColor(strokeA, strokeB, t),
              width:
                t === 0
                  ? a.stroke.width
                  : t === 1
                    ? b.stroke.width
                    : a.stroke.width + (b.stroke.width - a.stroke.width) * t,
              ...strokeStyle,
            },
          }
        : {}),
      ...(blockers.length ? { exportBlockers: blockers } : {}),
    });
  }
  return context.groupAppearance(values);
}
