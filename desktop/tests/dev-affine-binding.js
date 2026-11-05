// Trusted fixture handler; no script evaluation or special runtime.
export const fixtureEvaluate = (_operation, read) => read('x') * read('scale') + read('offset');
