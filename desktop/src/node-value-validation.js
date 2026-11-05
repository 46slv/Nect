// Shared pure predicate for authored runtime values and trusted declaration defaults.
export const isVector2 = (value) =>
  Array.isArray(value) && value.length === 2 && value.every(Number.isFinite);
