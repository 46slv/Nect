import { layoutText } from './text-layout.js';
import { validateTextWrap } from './text-wrap.js';
import { validateTextPathAttachment, validateTextPathMode, projectTextPath } from './text-path.js';
import { osFontTextPath } from './os-font-text.js';
import { fromPaper, getPaperScope } from './paper-runtime.js';
import { identityMatrix } from './character-geometry.js';
import { signature } from './path-signature.js';

export function evaluateText(_operation, read, context) {
  const n = context.textAuthorship(),
    fonts = context.textFonts;
  let result;
  if (n.osFont) {
    result = osFontTextPath(n, read('position'), read('size'), context.pathBounds);
    if (n.osFont.outline.wrapOverflow)
      context.warn('Wrap overflow; all artwork is retained without clipping');
    const reason =
      'OS font Text character adjustments and canvas caret editing are not supported; edit Text in Inspector';
    context.recordTextLayout({
      outlineUnits: [{ id: `${n.id}:osu0`, contours: result.contours.map((c) => c.id) }],
      characters: [],
      glyphAdvances: {},
      placements: [],
      lines: [],
      pairKerning: {},
      arrangementEligibility: { eligible: false, reason },
      reason,
    });
    result.characterOccurrences = [{ source: n.id, route: n.id, matrix: identityMatrix(), reason }];
    result.unstable = true;
    result.fingerprint = signature(result);
    return result;
  }
  const font = fonts[n.font];
  if (!font) throw Error('Font is still loading or unavailable');
  const attachment = n.pathAttachment;
  validateTextWrap(n.textWrap, n.writingMode, attachment);
  if (attachment) {
    validateTextPathAttachment(attachment);
    validateTextPathMode(n.text, n.writingMode);
  }
  let layout = layoutText(font, n.text, {
    position: attachment ? [0, 0] : read('position'),
    characterPositions: n.characterPositions,
    characterArrangement: n.characterArrangement,
    pathAttachment: attachment,
    fonts,
    fontId: n.font,
    variation: n.variation,
    fontFeatures: n.fontFeatures,
    size: read('size'),
    writingMode: n.writingMode,
    alignment: attachment ? 'start' : n.alignment,
    referenceBounds: attachment ? 'advance' : n.referenceBounds,
    crossAlignment: attachment ? 'origin' : n.crossAlignment,
    kerning: n.kerning,
    leading: n.leading,
    textWrap: n.textWrap,
    letterSpacing: n.params.letterSpacing ? read('letterSpacing') : 0,
    lineSpacing: n.params.lineSpacing ? read('lineSpacing') : 1.2,
  });
  if (attachment) {
    const source = context.textPathSource(attachment.nodeId);
    if (source.unstable)
      throw Error(
        'TEXT_PATH_UNSTABLE: Generated contour identities are unsupported; use a Curve or stable primitive',
      );
    if (source.remapSuspended)
      throw Error('TEXT_PATH_REMAP: Rebind suspended Path Remap edits before attaching');
    const contour = source.contours.find((c) => c.id === attachment.contourId);
    if (!contour)
      throw Error(
        'TEXT_PATH_CONTOUR: Selected contour is missing; detach or choose an explicit contour',
      );
    layout = projectTextPath(layout, contour, attachment, n.alignment || 'start');
  }
  for (const warning of layout.warnings) context.warn(warning);
  const item = new (getPaperScope().CompoundPath)({
    pathData: layout.path.toPathData(5),
    insert: false,
  });
  result = fromPaper(item, n.id);
  // Glyph command ranges identify outline units; keep them separate from paint groups.
  const unitCounts = layout.glyphs.map(
    (glyph) =>
      layout.path.commands
        .slice(glyph.commandStart, glyph.commandEnd)
        .filter((command) => command.type === 'M').length,
  );
  let contourOffset = 0;
  const outlineUnits =
    unitCounts.reduce((sum, count) => sum + count, 0) === result.contours.length
      ? unitCounts.flatMap((count, index) => {
          const contours = result.contours
            .slice(contourOffset, contourOffset + count)
            .map((c) => c.id);
          contourOffset += count;
          return count ? [{ id: `${n.id}:u${index}`, contours }] : [];
        })
      : [{ id: `${n.id}:u0`, contours: result.contours.map((c) => c.id) }];
  context.recordTextLayout({
    outlineUnits,
    arrangementStatus: layout.arrangementStatus,
    arrangementEligibility: layout.arrangementEligibility,
    characters: layout.characters,
    glyphAdvances: Object.fromEntries(
      layout.glyphs.map((glyph) => [glyph.sourceIndex, glyph.advance]),
    ),
    attachment: layout.attachment,
    placements: layout.placements,
    lineOffsets: layout.lineOffsets,
    lines: layout.lines,
    pairKerning: layout.pairKerning,
    reason: layout.characterReason,
    suspended: layout.characterSuspended,
  });
  result.characterOccurrences = [
    {
      source: n.id,
      route: n.id,
      matrix: identityMatrix(),
      reason:
        layout.characterReason ||
        (attachment
          ? 'Canvas character dragging is unavailable on Type on Path; use numeric local XY/rotation/scale'
          : null),
      arrangementStatus: layout.arrangementStatus,
      suspended: layout.characterSuspended,
    },
  ];
  result.unstable = true;
  result.fingerprint = signature(result);
  item.remove();
  return result;
}

// The host caches Text Position independently of outline generation.
export const projectTextOutput = (_output, value) => value;
