// Shared pure predicate for authored runtime values and trusted declaration defaults.
export const isVector2 = (value) =>
  Array.isArray(value) &&
  value.length === 2 &&
  Object.hasOwn(value, 0) &&
  Object.hasOwn(value, 1) &&
  Number.isFinite(value[0]) &&
  Number.isFinite(value[1]);
