import {
  identityMatrix,
  inverseAffineDelta,
  transformCharacterPoint,
} from './character-geometry.js';

export const TEXT_WRAP_WIDTH_LIMITS = [0.01, 100000];
const reject = (reason) => ({ supported: false, reason });
const driven = (n) =>
  Object.entries(n.params || {}).some(([key, q]) => key !== 'path' && (q.source || q.expression));

// Read only the selected capsule, using an already evaluated Canvas result.
// No document/evaluation cache mutation and no implicit source selection.
export function textWrapResizeContext(layer, result, fonts, sourceId = null) {
  try {
    if (!layer?.visible || layer.guide || layer.locked || result?.error)
      return reject('Unavailable artwork');
    const find = (id) => layer.nodes.find((n) => n.id === id);
    const route = [],
      matrix = identityMatrix();
    let n;
    if (sourceId) {
      n = find(sourceId);
      route.push(n);
    } else {
      const out = find(layer.output),
        fill = out && find(out.params?.path?.source);
      if (out?.type !== 'Output' || fill?.type !== 'Fill')
        return reject('Select a single solid Text artwork or source');
      route.push(out, fill);
      n = find(fill.params?.path?.source);
      if (n?.type === 'Transform') {
        route.push(n);
        if (driven(n)) return reject('Driven Transform');
        matrix.splice(0, 6, ...result.transformValues(n.id).matrix);
        n = find(n.params?.path?.source);
      }
      route.push(n);
    }
    if (route.some((n) => !n || n.locked || n.bypassed || driven(n)))
      return reject('Locked, bypassed or driven route');
    if (
      n?.type !== 'Text' ||
      n.osFont ||
      n.variation ||
      n.pathAttachment ||
      (n.writingMode || 'horizontal-tb') !== 'horizontal-tb' ||
      !n.textWrap ||
      !Object.hasOwn(n.textWrap, 'width') ||
      !['start', 'center', 'end'].includes(n.alignment || 'start') ||
      !['origin', 'start', 'center', 'end'].includes(n.crossAlignment || 'origin')
    )
      return reject('Unsupported Text wrap reference');
    const [a, b, c, d] = matrix,
      norm = Math.max(1, Math.abs(a), Math.abs(d));
    if (
      !matrix.every(Number.isFinite) ||
      a < 1e-6 ||
      d < 1e-6 ||
      Math.abs(b) > 1e-12 * norm ||
      Math.abs(c) > 1e-12 * norm
    )
      return reject('Use an axis-aligned positive Transform');
    inverseAffineDelta(matrix, [0, 0]);
    const layout = result.textLayout(n.id);
    if (layout?.reason || layout?.suspended || !layout?.lines?.length)
      return reject('Text layout unavailable');
    const values = result.objectResizeState(n.id),
      position = values.position;
    const styles = [{}, ...Object.values(n.characterPositions?.styles || {})];
    let ascent = 0,
      descent = 0;
    for (const style of styles) {
      const font = fonts[style.font || n.font],
        size = style.size ?? values.size,
        height = style.height ?? 1,
        shift = style.baselineShift ?? 0;
      if (!font) return reject('Font unavailable');
      ascent = Math.max(ascent, (font.ascender / font.unitsPerEm) * size * height + shift);
      descent = Math.max(descent, (-font.descender / font.unitsPerEm) * size * height - shift);
    }
    const width = n.textWrap.width,
      reference = n.alignment || 'start',
      referenceFactor = { start: 0, center: 0.5, end: 1 }[reference],
      left = position[0] - width * referenceFactor,
      right = left + width,
      crossReference = n.crossAlignment || 'origin';
    const height = Math.max(values.size, ascent + descent + Math.max(0, ...layout.lineOffsets)),
      // Layout already anchors the glyphs. Only position this logical metric frame.
      top =
        position[1] -
        (crossReference === 'origin'
          ? ascent
          : height * { start: 0, center: 0.5, end: 1 }[crossReference]);
    if (
      ![...position, width, top, height].every(Number.isFinite) ||
      width <= 0 ||
      width > TEXT_WRAP_WIDTH_LIMITS[1]
    )
      return reject('Invalid wrap frame');
    const corners = [
      [left, top],
      [right, top],
      [right, top + height],
      [left, top + height],
    ].map((p) => transformCharacterPoint(matrix, p));
    return {
      supported: true,
      layerId: layer.id,
      nodeId: n.id,
      width,
      reference,
      crossReference,
      matrix,
      routeIds: route.map((n) => n.id),
      corners,
      handle: transformCharacterPoint(matrix, [
        reference === 'end' ? left : right,
        top + height / 2,
      ]),
    };
  } catch (error) {
    return reject(error.message);
  }
}

export function resizedTextWrapWidth(width, matrix, delta, reference = 'start') {
  const factor = { start: 1, center: 2, end: -1 }[reference];
  if (
    !Number.isFinite(width) ||
    !Number.isFinite(factor) ||
    !Array.isArray(delta) ||
    delta.length !== 2 ||
    !delta.every(Number.isFinite)
  )
    throw Error('Invalid wrap resize');
  const local = inverseAffineDelta(matrix, delta);
  if (Math.abs(local[0]) < 1e-10) return width;
  return Math.max(
    TEXT_WRAP_WIDTH_LIMITS[0],
    Math.min(TEXT_WRAP_WIDTH_LIMITS[1], width + local[0] * factor),
  );
}
