import { interpolateClosedContours } from './blend-geometry.js';
import { colorOperation, publicColor } from './color-operations.js';

// First vertical: one solid-filled closed contour per source. Complex painted
// groups and strokes must not silently lose their appearance.
export function evaluateBlend(_operation, read, context) {
  const a = read('a'),
    b = read('b'),
    steps = read('steps');
  if (!a || !b) throw Error('Connect both Blend inputs');
  if (!Number.isInteger(steps) || steps < 0 || steps > 254)
    throw Error('Blend steps must be an integer from 0 to 254');
  for (const value of [a, b]) {
    if (value.kind || value.mask || value.stroke || value.contours?.length !== 1 || !value.fill)
      throw Error('Blend currently supports single solid-filled contours without stroke or mask');
    publicColor(colorOperation('mix', [value.fill, value.fill, 0]));
  }
  const values = [];
  for (let i = 0; i < steps + 2; i++) {
    const t = i / (steps + 1);
    values.push({
      contours: [
        interpolateClosedContours(a.contours[0], b.contours[0], t, `${context.nodeId}:blend:${i}`),
      ],
      fill: publicColor(colorOperation('mix', [a.fill, b.fill, t])),
    });
  }
  return context.groupAppearance(values);
}
