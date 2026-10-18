import {
  characterBoxPoints,
  characterTransform,
  multiplyMatrices,
  transformCharacterPoint,
} from './character-geometry.js';

// Carets follow the same evaluated outlines as the Canvas, including mixed fonts,
// character adjustments and downstream transforms. The native input stays transparent.
export function textEditGeometry(node, layout, occurrence, fonts = {}, defaults = {}) {
  const chars = Array.from(node.text.replace(/\r\n?/g, '\n'));
  const offsets = [0];
  for (const char of chars) offsets.push(offsets.at(-1) + char.length);
  const vertical = node.writingMode === 'vertical-rl';
  const matrix = occurrence.matrix;
  const carets = new Map();
  const glyphs = new Map((layout.glyphs || []).map((glyph) => [glyph.sourceIndex, glyph]));
  const size = defaults.size || node.params.size.value;
  const origin = defaults.position || node.params.position.value;
  const lineFor = (base, activeSize, font, width = 1, height = 1, transform = matrix) => {
    const asc = (font?.ascender ?? 800) / (font?.unitsPerEm ?? 1000);
    const desc = (font?.descender ?? -200) / (font?.unitsPerEm ?? 1000);
    const local = vertical
      ? [
          [base[0] - (activeSize * width) / 2, base[1]],
          [base[0] + (activeSize * width) / 2, base[1]],
        ]
      : [
          [base[0], base[1] - asc * activeSize * height],
          [base[0], base[1] - desc * activeSize * height],
        ];
    return local.map((point) => transformCharacterPoint(transform, point));
  };
  const selections = [];
  for (const glyph of layout.characters || []) {
    const start = offsets[glyph.sourceIndex],
      end = offsets[glyph.sourceIndex + 1];
    if (start === undefined || end === undefined) continue;
    const shape = characterTransform(glyph.pivot, glyph.offset, glyph.scale, glyph.rotation);
    const combined = multiplyMatrices(matrix, shape);
    const advance =
      layout.glyphAdvances?.[glyph.sourceIndex] ??
      glyph.advance ??
      glyphs.get(glyph.sourceIndex)?.advance ??
      glyph.size;
    const after = [...glyph.base];
    after[vertical ? 1 : 0] += advance;
    carets.set(start, {
      index: start,
      points: lineFor(
        glyph.base,
        glyph.size,
        fonts[glyph.font],
        glyph.width,
        glyph.height,
        combined,
      ),
    });
    if (!carets.has(end))
      carets.set(end, {
        index: end,
        points: lineFor(after, glyph.size, fonts[glyph.font], glyph.width, glyph.height, combined),
      });
    selections.push({ start, end, points: characterBoxPoints(glyph, matrix) });
  }
  // Empty text/lines still have an editable insertion position.
  const first = layout.characters?.[0];
  const firstLine = (layout.lines || []).findIndex(
    (line) => first && first.sourceIndex >= line.sourceStart && first.sourceIndex < line.sourceEnd,
  );
  for (const [i, line] of (layout.lines?.length
    ? layout.lines
    : [{ sourceStart: 0, sourceEnd: 0 }]
  ).entries()) {
    const index = offsets[line.sourceStart] ?? node.text.length;
    if (carets.has(index)) continue;
    const base = [...origin];
    const distance = layout.lineOffsets?.[i] ?? i * size * (defaults.lineSpacing || 1.2);
    if (vertical)
      base[0] = (first?.base[0] ?? origin[0]) - distance + (layout.lineOffsets?.[firstLine] || 0);
    else
      base[1] = (first?.base[1] ?? origin[1]) + distance - (layout.lineOffsets?.[firstLine] || 0);
    carets.set(index, { index, points: lineFor(base, size, fonts[node.font]) });
  }
  return { carets: [...carets.values()].sort((a, b) => a.index - b.index), selections };
}

export function nearestTextCaret(geometry, point) {
  let nearest = 0,
    best = Infinity;
  for (const {
    index,
    points: [a, b],
  } of geometry.carets) {
    const vector = b.map((v, i) => v - a[i]);
    const length = vector[0] ** 2 + vector[1] ** 2;
    const t = Math.max(
      0,
      Math.min(
        1,
        length ? ((point[0] - a[0]) * vector[0] + (point[1] - a[1]) * vector[1]) / length : 0,
      ),
    );
    const distance = Math.hypot(...point.map((v, i) => v - a[i] - t * vector[i]));
    if (distance < best) {
      best = distance;
      nearest = index;
    }
  }
  return nearest;
}
