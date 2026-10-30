import {
  validateTextWrap,
  wrapExtent,
  wrapBoundaryAllowed,
  JAPANESE_WRAP_POLICY,
} from './text-wrap.js';
import { parseOsContours } from './os-font-outline.js';
import {
  PRIVATE_FONT_SCHEMA_VERSION,
  validatePrivateFontSource,
  samePrivateFontSource,
} from './private-font-source.js';
const exact = (value, keys) =>
  value &&
  Object.getPrototypeOf(value) === Object.prototype &&
  Object.keys(value).length === keys.length &&
  Object.keys(value).every((k) => keys.includes(k));
export function normalizeDesktopText(text) {
  if (typeof text !== 'string' || text.length > 4096) throw Error('Text exceeds 4096 UTF-16 units');
  const value = text.replace(/\r\n?/g, '\n').normalize('NFC');
  for (const char of value) {
    const n = char.codePointAt(0);
    if (
      (n < 32 && n !== 9 && n !== 10) ||
      (n >= 127 && n <= 159) ||
      (n >= 0xd800 && n <= 0xdfff) ||
      (n & 0xffff) >= 0xfffe
    )
      throw Error('Unsupported control or invalid Unicode scalar');
  }
  if (value.split('\n').length > 256) throw Error('Text exceeds 256 lines');
  return value;
}
export function validateDesktopFace(face) {
  if (
    !exact(face, ['family', 'name', 'weight', 'style', 'stretch', 'format', 'color']) ||
    ![face.family, face.name].every(
      (s) => typeof s === 'string' && s.trim() && s.length <= 255 && !/[\u0000-\u001f]/.test(s),
    ) ||
    !Number.isInteger(face.weight) ||
    face.weight < 1 ||
    face.weight > 999 ||
    !Number.isInteger(face.style) ||
    face.style < 0 ||
    face.style > 2 ||
    !Number.isInteger(face.stretch) ||
    face.stretch < 1 ||
    face.stretch > 9 ||
    !Number.isInteger(face.format) ||
    face.format < 0 ||
    face.format > 7 ||
    typeof face.color !== 'boolean'
  )
    throw Error('Invalid Windows font face');
  if (face.color) throw Error('Color/bitmap font outlines are not supported');
}
export function validateDesktopAxes(axes) {
  const tags = new Set();
  if (!Array.isArray(axes) || axes.length > 32) throw Error('Invalid Windows font axes');
  for (const a of axes) {
    if (
      !exact(a, ['tag', 'name', 'min', 'max', 'defaultValue', 'baseValue', 'value', 'hidden']) ||
      typeof a.tag !== 'string' ||
      !/^[\x20-\x7e]{4}$/.test(a.tag) ||
      tags.has(a.tag) ||
      typeof a.name !== 'string' ||
      !a.name.trim() ||
      a.name.length > 255 ||
      /[\u0000-\u001f\u007f-\u009f]/.test(a.name) ||
      typeof a.hidden !== 'boolean' ||
      ![a.min, a.max, a.defaultValue, a.baseValue, a.value].every(
        (v) => Number.isFinite(v) && Math.abs(v) <= 1000000,
      ) ||
      a.min >= a.max ||
      [a.defaultValue, a.baseValue, a.value].some((v) => v < a.min || v > a.max)
    )
      throw Error('Invalid Windows font-axis range or value');
    tags.add(a.tag);
  }
  return Object.fromEntries(axes.map((a) => [a.tag, a.value]));
}
export function desktopTextIntent(n, face, patch = {}, sourceIdentity) {
  validateDesktopFace(face);
  if (sourceIdentity !== undefined) validatePrivateFontSource(sourceIdentity);
  if (n?.type !== 'Text' || n.variation || n.pathAttachment || n.leading || n.characterArrangement)
    throw Error('Windows Text requires text without character/path/asset-variable adjustments');
  if (
    ['offsets', 'styles', 'transforms', 'kerningPairs'].some(
      (k) => Object.keys(n.characterPositions?.[k] || {}).length,
    )
  )
    throw Error('Remove character overrides before choosing a Windows font');
  for (const key of ['size', 'letterSpacing', 'lineSpacing'])
    if (n.params[key].source || n.params[key].expression)
      throw Error('Windows Text size and spacing cannot be driven');
  const text = normalizeDesktopText(patch.text ?? n.text),
    size = patch.size ?? n.params.size.value,
    tracking = patch.tracking ?? n.params.letterSpacing.value,
    lineSpacing = patch.lineSpacing ?? n.params.lineSpacing.value,
    kerning = patch.kerning ?? n.kerning ?? true,
    ligatures = patch.ligatures ?? n.osFont?.outline.ligatures ?? true,
    alignment = ['start', 'center', 'end'].indexOf(patch.alignment ?? n.alignment ?? 'start');
  if (
    !Number.isFinite(size) ||
    size < 0.01 ||
    size > 10000 ||
    !Number.isFinite(tracking) ||
    Math.abs(tracking) > 10000 ||
    !Number.isFinite(lineSpacing) ||
    lineSpacing < 0.1 ||
    lineSpacing > 100 ||
    typeof kerning !== 'boolean' ||
    typeof ligatures !== 'boolean' ||
    alignment < 0
  )
    throw Error('Invalid Windows Text size, spacing or typography settings');
  const result = { face, text, size, tracking, lineSpacing, kerning, ligatures, alignment };
  const writingMode = patch.writingMode ?? n.writingMode ?? 'horizontal-tb';
  if (!['horizontal-tb', 'vertical-rl'].includes(writingMode))
    throw Error('Unsupported Windows Text writing mode');
  const wrap = Object.hasOwn(patch, 'textWrap') ? (patch.textWrap ?? undefined) : n.textWrap;
  validateTextWrap(wrap, writingMode, n.pathAttachment);
  if (wrap && Math.fround(wrapExtent(wrap)) <= 0) throw Error('Windows wrap width is too small');
  const retained =
    n.osFont?.version >= 3 &&
    equalFace(n.osFont.face, face) &&
    samePrivateFontSource(n.osFont.source, sourceIdentity);
  if (retained || wrap || patch.axes !== undefined || writingMode === 'vertical-rl') {
    const axes = patch.axes ?? (retained ? validateDesktopAxes(n.osFont.axes) : {});
    if (
      !axes ||
      Object.getPrototypeOf(axes) !== Object.prototype ||
      Object.keys(axes).length > 32 ||
      Object.entries(axes).some(
        ([tag, v]) => !/^[\x20-\x7e]{4}$/.test(tag) || !Number.isFinite(v) || Math.abs(v) > 1000000,
      )
    )
      throw Error('Invalid Windows Text axis request');
    return {
      ...result,
      axes: { ...axes },
      writingMode,
      ...(wrap
        ? {
            ...(wrap.width !== undefined ? { wrapWidth: wrap.width } : { wrapHeight: wrap.height }),
            ...(wrap.policy ? { wrapPolicy: wrap.policy } : {}),
          }
        : {}),
    };
  }
  return result;
}
const equalFace = (a, b) =>
  ['family', 'name', 'weight', 'style', 'stretch', 'format', 'color'].every((k) => a[k] === b[k]);
/** Exact original UTF-16 line spans; user newlines remain authoritative. */
export function validateDesktopLineSpans(raw) {
  if (
    !Array.isArray(raw.lineSpans) ||
    raw.lineSpans.length !== raw.lineCount ||
    !raw.lineSpans.length
  )
    throw Error('Invalid Windows line spans');
  let end = 0;
  const scalarBoundary = (i) =>
    !(
      i > 0 &&
      i < raw.text.length &&
      /[\uD800-\uDBFF]/.test(raw.text[i - 1]) &&
      /[\uDC00-\uDFFF]/.test(raw.text[i])
    );
  for (const [i, row] of raw.lineSpans.entries()) {
    if (
      !exact(row, ['start', 'length', 'trailingWhitespaceLength', 'newlineLength']) ||
      Object.values(row).some((v) => !Number.isInteger(v) || v < 0) ||
      row.start !== end ||
      row.newlineLength > 1 ||
      row.newlineLength > row.trailingWhitespaceLength ||
      row.trailingWhitespaceLength > row.length ||
      (!row.length && i + 1 !== raw.lineSpans.length)
    )
      throw Error('Invalid Windows line span');
    end = row.start + row.length;
    if (end > raw.text.length || !scalarBoundary(end))
      throw Error('Windows line splits a Unicode scalar');
    const part = raw.text.slice(row.start, end);
    if (
      part.endsWith('\n') !== (row.newlineLength === 1) ||
      part.slice(0, part.length - row.newlineLength).includes('\n')
    )
      throw Error('Invalid Windows hard line span');
    if (!row.newlineLength && end < raw.text.length) {
      const remainder = raw.text.slice(end),
        trimmedLeft = part.replace(/[ \t\u3000]+$/u, ''),
        trimmedRight = remainder.replace(/^[ \t\u3000]+/u, ''),
        left = [...trimmedLeft].at(-1),
        right = [...trimmedRight][0],
        separated = trimmedLeft.length !== part.length || trimmedRight.length !== remainder.length;
      if (!wrapBoundaryAllowed(left, right, JAPANESE_WRAP_POLICY, separated))
        throw Error('Windows line break violates Japanese basic wrapping');
    }
  }
  if (end !== raw.text.length) throw Error('Incomplete Windows line spans');
}
export function validateDesktopText(n, schemaVersion = 35) {
  const ref = n.osFont;
  const basic = ref?.version === 5,
    wrapped = ref?.version >= 4,
    extended = ref?.version >= 3;
  if (
    schemaVersion < (basic ? 34 : wrapped ? 32 : extended ? 30 : 29) ||
    n.type !== 'Text' ||
    n.font !== 'default' ||
    !exact(ref, [
      'version', 'family', 'style', 'face', 'outline',
      ...(extended ? ['axes'] : []),
      ...(ref?.source !== undefined ? ['source'] : []),
    ]) ||
    ![2, 3, 4, 5].includes(ref.version)
  )
    throw Error('Invalid shaped Windows Text schema');
  if (ref.source !== undefined) {
    if (schemaVersion < PRIVATE_FONT_SCHEMA_VERSION)
      throw Error('Private font Text requires project schema 35');
    validatePrivateFontSource(ref.source);
  }
  validateDesktopFace(ref.face);
  if (ref.family !== ref.face.family || ref.style !== ref.face.style)
    throw Error('Unmatched Windows Text reference');
  const raw = ref.outline;
  if (
    !exact(raw, [
      'engine',
      'face',
      'text',
      'size',
      'tracking',
      'lineSpacing',
      'kerning',
      'ligatures',
      'alignment',
      'points',
      'types',
      'ascent',
      'advanceWidth',
      'lineCount',
      'glyphCount',
      ...(extended ? ['axes', 'writingMode', 'advanceHeight'] : []),
      ...(wrapped
        ? [basic && raw.writingMode === 'vertical-rl' ? 'wrapHeight' : 'wrapWidth', 'wrapOverflow']
        : []),
      ...(basic ? ['wrapPolicy', 'lineSpans'] : []),
    ]) ||
    raw.engine !==
      (basic
        ? 'directwrite-v4'
        : wrapped
          ? 'directwrite-v3'
          : extended
            ? 'directwrite-v2'
            : 'directwrite-v1') ||
    !equalFace(raw.face, ref.face)
  )
    throw Error('Invalid shaped Windows outline snapshot');
  const intent = desktopTextIntent(n, ref.face, {}, ref.source);
  if ((intent.wrapPolicy === JAPANESE_WRAP_POLICY) !== basic)
    throw Error('Windows wrapping policy needs fresh preparation');
  const extentKey = basic && raw.writingMode === 'vertical-rl' ? 'wrapHeight' : 'wrapWidth',
    advanceKey = extentKey === 'wrapHeight' ? 'advanceHeight' : 'advanceWidth';
  if (
    wrapped
      ? !Number.isFinite(raw[extentKey]) ||
        raw[extentKey] <= 0 ||
        raw[extentKey] > 100000 ||
        !Number.isFinite(intent[extentKey]) ||
        Math.fround(raw[extentKey]) !== Math.fround(intent[extentKey]) ||
        typeof raw.wrapOverflow !== 'boolean' ||
        raw.wrapOverflow !==
          Math.fround(raw[advanceKey]) > Math.fround(raw[extentKey] + Math.fround(0.01))
      : intent.wrapWidth !== undefined || intent.wrapHeight !== undefined
  )
    throw Error('Windows wrap needs fresh preparation');
  if (basic) {
    if (raw.wrapPolicy !== JAPANESE_WRAP_POLICY || intent.wrapPolicy !== raw.wrapPolicy)
      throw Error('Windows wrapping policy needs fresh preparation');
    validateDesktopLineSpans(raw);
  }
  if (extended) {
    validateDesktopAxes(ref.axes);
    validateDesktopAxes(raw.axes);
    if (
      raw.writingMode !== intent.writingMode ||
      !Number.isFinite(raw.advanceHeight) ||
      raw.advanceHeight < 0 ||
      raw.advanceHeight > 10000000 ||
      raw.axes.length !== ref.axes.length ||
      raw.axes.some((a) => {
        const source = ref.axes.find((b) => b.tag === a.tag);
        return !source || Object.keys(a).some((k) => a[k] !== source[k]);
      })
    )
      throw Error('Windows axes or writing mode need fresh preparation');
  } else if ((n.writingMode ?? 'horizontal-tb') !== 'horizontal-tb')
    throw Error('Windows writing mode needs fresh preparation');
  if (
    raw.text !== intent.text ||
    ['size', 'tracking', 'lineSpacing'].some(
      (k) => !Number.isFinite(raw[k]) || Math.fround(raw[k]) !== Math.fround(intent[k]),
    ) ||
    raw.kerning !== intent.kerning ||
    raw.alignment !== intent.alignment ||
    typeof raw.ligatures !== 'boolean' ||
    !Number.isFinite(raw.ascent) ||
    raw.ascent < 0 ||
    raw.ascent > raw.size * 10 ||
    !Number.isFinite(raw.advanceWidth) ||
    raw.advanceWidth < 0 ||
    raw.advanceWidth > 10000000 ||
    !Number.isInteger(raw.lineCount) ||
    raw.lineCount < 1 ||
    raw.lineCount > 256 ||
    !Number.isInteger(raw.glyphCount) ||
    raw.glyphCount < 0 ||
    raw.glyphCount > 20000 ||
    (!raw.text.trim() && raw.points?.length) ||
    (raw.text.trim() && !raw.points?.length)
  )
    throw Error('Windows text or settings need fresh preparation');
  let id = 0;
  return parseOsContours(raw.points, raw.types, (prefix) => `${n.id}:dw:${prefix}${id++}`, {
    rawBudget: 300000,
    vertexBudget: 100000,
  });
}
export function desktopFontTextPath(n, position, size, boundsOf) {
  const path = validateDesktopText(n);
  if (Math.fround(size) !== Math.fround(n.osFont.outline.size) || !position?.every(Number.isFinite))
    throw Error('Windows Text evaluated settings need fresh preparation');
  const raw = n.osFont.outline,
    bounds = boundsOf(path),
    vertical = raw.writingMode === 'vertical-rl',
    factor = n.alignment === 'center' ? 1 : n.alignment === 'end' ? 2 : 0;
  const x = vertical
      ? n.crossAlignment && n.crossAlignment !== 'origin'
        ? bounds[0][n.crossAlignment === 'center' ? 1 : n.crossAlignment === 'end' ? 2 : 0]
        : raw.advanceWidth - raw.ascent
      : n.referenceBounds === 'advance'
        ? (raw.wrapWidth ?? raw.advanceWidth) *
          (n.alignment === 'center' ? 0.5 : n.alignment === 'end' ? 1 : 0)
        : bounds[0][n.alignment === 'center' ? 1 : n.alignment === 'end' ? 2 : 0],
    y = vertical
      ? n.referenceBounds === 'advance'
        ? ((raw.wrapHeight ?? raw.advanceHeight) * factor) / 2
        : bounds[1][factor]
      : n.crossAlignment && n.crossAlignment !== 'origin'
        ? bounds[1][n.crossAlignment === 'center' ? 1 : n.crossAlignment === 'end' ? 2 : 0]
        : raw.ascent;
  for (const c of path.contours)
    for (const pt of c.points)
      pt.position = pt.position.map((v, k) => v + position[k] - (k ? y : x));
  return path;
}
export function desktopOsReference(raw, source) {
  if (source !== undefined) validatePrivateFontSource(source);
  return {
    version:
      raw.engine === 'directwrite-v4'
        ? 5
        : raw.engine === 'directwrite-v3'
          ? 4
          : raw.engine === 'directwrite-v2'
            ? 3
            : 2,
    family: raw.face.family,
    style: raw.face.style,
    face: structuredClone(raw.face),
    outline: structuredClone(raw),
    ...(source === undefined ? {} : { source: structuredClone(source) }),
    ...(['directwrite-v2', 'directwrite-v3', 'directwrite-v4'].includes(raw.engine)
      ? { axes: structuredClone(raw.axes) }
      : {}),
  };
}
