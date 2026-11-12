export function evaluateOutput(_operation, read, context) {
  if (context.hasEmptyOutput()) return { contours: [] };
  const source = read('path');
  if (!source) throw Error('Connect a PaintedPath input');
  return context.cloneEvaluated(source);
}
