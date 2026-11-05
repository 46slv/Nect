export function evaluateVectorMath(operation, read) {
  const a = read('a');
  if (operation === 'length') return Math.hypot(...a);
  if (operation === 'normalize') {
    // Scale before measuring so both huge and subnormal finite inputs normalize safely.
    const largest = Math.max(...a.map(Math.abs));
    if (largest === 0) return [0, 0];
    const scaled = a.map((v) => v / largest),
      length = Math.hypot(...scaled);
    return scaled.map((v) => v / length);
  }
  if (operation === 'scale') {
    const scale = read('scale');
    return a.map((v) => v * scale);
  }
  const b = read('b');
  if (operation === 'distance') return Math.hypot(a[0] - b[0], a[1] - b[1]);
  if (operation === 'dot') return a[0] * b[0] + a[1] * b[1];
  return a.map((v, i) =>
    ({
      add: () => v + b[i],
      subtract: () => v - b[i],
      multiply: () => v * b[i],
      divide: () => (b[i] === 0 ? 0 : v / b[i]),
    })[operation](),
  );
}
