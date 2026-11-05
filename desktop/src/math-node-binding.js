// Trusted builtin handler. Inputs remain lazy and are owned by the existing evaluator.
export function evaluateMath(operation, read) {
  const a = read('a');
  if (operation === 'abs') return Math.abs(a);
  if (operation === 'clamp') {
    const min = read('min'),
      max = read('max');
    if (min > max) throw Error('Clamp minimum exceeds maximum');
    return Math.min(max, Math.max(min, a));
  }
  const b = read('b');
  const operations = {
    add: () => a + b,
    subtract: () => a - b,
    multiply: () => a * b,
    divide: () => (b === 0 ? 0 : a / b),
    min: () => Math.min(a, b),
    max: () => Math.max(a, b),
  };
  if (!Object.hasOwn(operations, operation)) throw Error('Invalid Math operation');
  return operations[operation]();
}
