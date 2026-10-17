import opentype from 'opentype.js';
import { requireThat as check } from './contract.js';
import { validateVariableText, variableRequest } from './document-bridge.js';

/** Consume the worker's already positioned Y-down design-unit outlines exactly once. */
export function layoutPreparedVariable(font, text, options) {
  const {
    position,
    size,
    letterSpacing = 0,
    alignment = 'start',
    referenceBounds = 'advance',
    crossAlignment = 'origin',
    characterPositions,
    fontId,
    variation,
  } = options;
  validateVariableText(
    { ...options, type: 'Text', font: fontId, variation, text },
    { kind: 'variable-glyf-v1' },
  );
  const run = font.readVariable(variableRequest({ text, variation }, font));
  const scale = size / run.unitsPerEm;
  const path = new opentype.Path();
  let pen = 0;
  const glyphs = run.glyphs.map((g, index) => {
    const x = position[0] + pen * scale + index * letterSpacing;
    for (const command of run.commands.slice(g.commandStart, g.commandEnd)) {
      const next = { ...command };
      for (const suffix of ['', '1', '2'])
        if (`x${suffix}` in command) {
          next[`x${suffix}`] = position[0] + command[`x${suffix}`] * scale + index * letterSpacing;
          next[`y${suffix}`] = position[1] + command[`y${suffix}`] * scale;
        }
      path.commands.push(next);
    }
    const glyph = {
      id: g.id,
      char: text[index],
      sourceIndex: index,
      line: 0,
      x,
      y: position[1],
      rotation: 0,
      advance: g.xAdvance * scale,
      fontId,
      size,
      width: 1,
      height: 1,
      tracking: letterSpacing,
      baselineShift: 0,
      metricCenter: 0,
      commandStart: g.commandStart,
      commandEnd: g.commandEnd,
    };
    pen += g.xAdvance;
    return glyph;
  });
  const advance = run.advance * scale + Math.max(0, glyphs.length - 1) * letterSpacing;
  const factor = { start: 0, center: 0.5, end: 1 }[alignment];
  const box = path.getBoundingBox();
  let dx = -advance * factor,
    dy = 0;
  if (referenceBounds === 'visible' && path.commands.length)
    dx = position[0] - (box.x1 + (box.x2 - box.x1) * factor);
  if (crossAlignment !== 'origin' && path.commands.length)
    dy =
      position[1] -
      (box.y1 + (box.y2 - box.y1) * { start: 0, center: 0.5, end: 1 }[crossAlignment]);
  for (const command of path.commands)
    for (const suffix of ['', '1', '2'])
      if (`x${suffix}` in command) {
        command[`x${suffix}`] += dx;
        command[`y${suffix}`] += dy;
        check(
          Number.isFinite(command[`x${suffix}`]) && Number.isFinite(command[`y${suffix}`]),
          'OUTPUT_COORDINATE',
        );
      }
  const characters = glyphs.map((g, i) => {
    g.x += dx;
    g.y += dy;
    const outline = new opentype.Path();
    outline.commands = path.commands.slice(g.commandStart, g.commandEnd);
    const b = outline.getBoundingBox(),
      whitespace = !outline.commands.length;
    return {
      id: characterPositions?.basis === text ? characterPositions.units[i].id : `c${i + 1}`,
      text: g.char,
      font: fontId,
      size,
      width: 1,
      height: 1,
      tracking: letterSpacing,
      baselineShift: 0,
      sourceIndex: i,
      bounds: whitespace
        ? [g.x, g.y - size * 0.8, g.x + Math.max(g.advance, size * 0.2), g.y + size * 0.2]
        : [b.x1, b.y1, b.x2, b.y2],
      base: [g.x, g.y],
      pivot: [g.x + g.advance / 2, g.y],
      rotation: 0,
      scale: 1,
      offset: [0, 0],
      whitespace,
      path: outline.toPathData(5),
    };
  });
  const reason =
    'VARIABLE_RANGE_UNSUPPORTED: Variable fonts currently support whole horizontal Text only';
  return {
    path,
    glyphs,
    characters,
    advances: [advance],
    lineOffsets: [0],
    lines: [{ sourceStart: 0, sourceEnd: text.length, advance }],
    pairKerning: {},
    warnings: [],
    characterReason: reason,
    characterSuspended: false,
    arrangementEligibility: { eligible: false, reason },
    arrangementStatus: { active: false, suspended: false, reason },
  };
}
