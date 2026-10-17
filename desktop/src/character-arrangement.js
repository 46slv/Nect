import opentype from 'opentype.js';
import { normalizeCharacterText } from './character-identity.js';

export function validateCharacterArrangement(value) {
  if (value === undefined) return;
  if (
    !value ||
    typeof value !== 'object' ||
    Array.isArray(value) ||
    Object.keys(value).some(
      (k) => !['version', 'mode', 'axis', 'ids', 'gap', 'anchorOffset'].includes(k),
    ) ||
    value.version !== 1 ||
    value.mode !== 'equal-ink-gap' ||
    !['x', 'y'].includes(value.axis) ||
    !Array.isArray(value.ids) ||
    value.ids.length < 2 ||
    value.ids.length > 10000 ||
    new Set(value.ids).size !== value.ids.length ||
    value.ids.some((id) => typeof id !== 'string' || !/^c[1-9]\d{0,15}$/.test(id)) ||
    !Number.isFinite(value.gap) ||
    value.gap < 0 ||
    value.gap > 100000 ||
    !Number.isFinite(value.anchorOffset) ||
    Math.abs(value.anchorOffset) > 100000
  )
    throw Error('Invalid character arrangement version, membership, gap or anchor');
}

/** Fully transformed, unrounded ink bounds; no synthesized whitespace cells. */
export function arrangementEligibility({
  text,
  record,
  characters,
  glyphs,
  reason,
  textWrap,
  pathAttachment,
}) {
  let error = reason;
  if (pathAttachment) error = 'Type on Path is unsupported for maintained ink gaps';
  else if (textWrap) error = 'Disable text wrap before maintaining ink gaps';
  else if (/\s/u.test(normalizeCharacterText(text)))
    error = 'Maintained ink gaps require one line or column without whitespace';
  else if (!error && characters.length < 2)
    error = 'Maintained ink gaps require at least two outlined characters';
  else if (!error && record && record.basis !== normalizeCharacterText(text))
    error = 'Character identity basis does not match current text';
  else if (
    !error &&
    (characters.length !== [...normalizeCharacterText(text)].length ||
      glyphs.length !== characters.length ||
      characters.some(
        (c, i) =>
          c.sourceIndex !== i ||
          glyphs[i].sourceIndex !== i ||
          c.whitespace ||
          !c.bounds.every(Number.isFinite) ||
          (record && (record.units[i]?.id !== c.id || record.units[i]?.text !== c.text)),
      ))
  )
    error = 'Character identity or outline mapping is unavailable';
  return { eligible: !error, reason: error || null };
}

export function applyCharacterArrangement({
  intent,
  eligibility,
  record,
  characters,
  glyphs,
  path,
  position,
  writingMode,
}) {
  if (!intent) return { active: false, suspended: false, reason: null };
  validateCharacterArrangement(intent);
  let reason = eligibility.reason;
  if (
    !reason &&
    (!record ||
      intent.ids.length !== characters.length ||
      intent.ids.some((id, i) => id !== characters[i].id))
  )
    reason = 'Character membership or order changed; reapply to current text';
  const axis = writingMode === 'vertical-rl' ? 'y' : 'x';
  if (!reason && intent.axis !== axis) reason = 'Writing mode changed; reapply to current text';
  if (reason) return { active: false, suspended: true, reason };
  const index = axis === 'x' ? 0 : 1;
  let start = position[index] + intent.anchorOffset;
  const deltas = characters.map((c) => {
    const delta = start - c.bounds[index];
    start += c.bounds[index + 2] - c.bounds[index] + intent.gap;
    return delta;
  });
  // Validate every resulting coordinate before changing any command.
  if (
    !Number.isFinite(start) ||
    deltas.some(
      (delta, i) =>
        !Number.isFinite(delta) ||
        path.commands
          .slice(glyphs[i].commandStart, glyphs[i].commandEnd)
          .some((cmd) =>
            ['', '1', '2'].some((s) => axis + s in cmd && !Number.isFinite(cmd[axis + s] + delta)),
          ),
    )
  )
    return { active: false, suspended: true, reason: 'Maintained ink gap coordinates overflow' };
  characters.forEach((c, i) => {
    const glyph = glyphs[i],
      delta = deltas[i];
    for (let j = glyph.commandStart; j < glyph.commandEnd; j++)
      for (const suffix of ['', '1', '2'])
        if (axis + suffix in path.commands[j]) path.commands[j][axis + suffix] += delta;
    c.bounds[index] += delta;
    c.bounds[index + 2] += delta;
    c.arrangementOffset = index === 0 ? [delta, 0] : [0, delta];
    const outline = new opentype.Path();
    outline.commands = path.commands.slice(glyph.commandStart, glyph.commandEnd);
    c.path = outline.toPathData(5);
  });
  return { active: true, suspended: false, reason: null };
}
