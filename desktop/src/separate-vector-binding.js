// Cache the original vector once; projection is selected after the shared cache.
export const evaluateSeparateVector = (_operation, read) => read('vector');
export const projectSeparateVector = (output, value) => value[output === 'x' ? 0 : 1];
