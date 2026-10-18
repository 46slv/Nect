import { parseOsContours } from './os-font-outline.js';
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
export function desktopTextIntent(n, face, patch = {}) {
  validateDesktopFace(face);
  if (
    n?.type !== 'Text' ||
    n.variation ||
    n.pathAttachment ||
    n.textWrap ||
    n.leading ||
    n.characterArrangement ||
    n.writingMode === 'vertical-rl'
  )
    throw Error(
      'Windows Text currently supports horizontal unwrapped text without character/path/variable adjustments',
    );
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
  return { face, text, size, tracking, lineSpacing, kerning, ligatures, alignment };
}
const equalFace = (a, b) =>
  ['family', 'name', 'weight', 'style', 'stretch', 'format', 'color'].every((k) => a[k] === b[k]);
export function validateDesktopText(n, schemaVersion = 29) {
  const ref = n.osFont;
  if (
    schemaVersion < 29 ||
    n.type !== 'Text' ||
    n.font !== 'default' ||
    !exact(ref, ['version', 'family', 'style', 'face', 'outline']) ||
    ref.version !== 2
  )
    throw Error('Invalid shaped Windows Text schema');
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
    ]) ||
    raw.engine !== 'directwrite-v1' ||
    !equalFace(raw.face, ref.face)
  )
    throw Error('Invalid shaped Windows outline snapshot');
  const intent = desktopTextIntent(n, ref.face);
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
    x =
      n.referenceBounds === 'advance'
        ? raw.advanceWidth * (n.alignment === 'center' ? 0.5 : n.alignment === 'end' ? 1 : 0)
        : bounds[0][n.alignment === 'center' ? 1 : n.alignment === 'end' ? 2 : 0],
    y =
      n.crossAlignment && n.crossAlignment !== 'origin'
        ? bounds[1][n.crossAlignment === 'center' ? 1 : n.crossAlignment === 'end' ? 2 : 0]
        : raw.ascent;
  for (const c of path.contours)
    for (const pt of c.points)
      pt.position = pt.position.map((v, k) => v + position[k] - (k ? y : x));
  return path;
}
export function desktopOsReference(raw) {
  return {
    version: 2,
    family: raw.face.family,
    style: raw.face.style,
    face: structuredClone(raw.face),
    outline: structuredClone(raw),
  };
}
