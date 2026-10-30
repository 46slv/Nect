// Persist only authored identity. File paths, bytes and session capabilities
// belong to the native process, never to a project or browser preference.
export const PRIVATE_FONT_SCHEMA_VERSION = 35;
export const PRIVATE_FONT_SOURCE_KIND = 'file-native-v1';

export function validatePrivateFontSource(source) {
  if (
    !source ||
    Object.getPrototypeOf(source) !== Object.prototype ||
    Object.keys(source).length !== 3 ||
    Object.keys(source).some((key) => !['kind', 'sha256', 'faceIndex'].includes(key)) ||
    source.kind !== PRIVATE_FONT_SOURCE_KIND ||
    typeof source.sha256 !== 'string' ||
    !/^[a-f0-9]{64}$/.test(source.sha256) ||
    !Number.isInteger(source.faceIndex) ||
    source.faceIndex < 0 ||
    source.faceIndex >= 256
  )
    throw Error('Invalid private font source');
  return source;
}

export function samePrivateFontSource(a, b) {
  if (a === undefined || b === undefined) return a === b;
  validatePrivateFontSource(a);
  validatePrivateFontSource(b);
  return a.kind === b.kind && a.sha256 === b.sha256 && a.faceIndex === b.faceIndex;
}

export function privateFontChoiceId(source) {
  validatePrivateFontSource(source);
  return JSON.stringify([source.kind, source.sha256, source.faceIndex]);
}
