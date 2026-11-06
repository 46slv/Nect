// Preserve the legacy input order and leave source/fallback resolution to the host.
export const evaluateCombineVector = (_operation, read) => [read('x'), read('y')];
