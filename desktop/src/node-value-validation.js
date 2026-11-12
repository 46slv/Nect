// Shared pure predicate for authored runtime values and trusted declaration defaults.
export const isVector2 = (value) =>
  Array.isArray(value) && value.length === 2 && value.every(Number.isFinite);

export const isColor = (value) =>
  !!value &&
  typeof value === 'object' &&
  !Array.isArray(value) &&
  Object.keys(value).length === 2 &&
  typeof value.hex === 'string' &&
  /^#[0-9a-fA-F]{6}$/.test(value.hex) &&
  Number.isFinite(value.alpha) &&
  value.alpha >= 0 &&
  value.alpha <= 1;
