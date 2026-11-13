import { isPaintedGroup } from './composition-reference.js';
import { createLinearGradientFill } from './linear-gradient.js';

const keys = ['start', 'end', 'startColor', 'endColor'];
const colorKeys = Object.freeze(['startColor', 'endColor']);

export function linearGradientSettings(read) {
  return createLinearGradientFill(Object.fromEntries(keys.map((key) => [key, read(key)])));
}

export function evaluateLinearGradient(_operation, read, context) {
  const source = read('path');
  if (!source) throw Error('Connect a PaintedPath input');
  const result = context.cloneEvaluated(source);
  if (isPaintedGroup(result))
    throw Error('Edit paint in the source Composition, or use Convert to Path');
  result.fill = linearGradientSettings(read);
  context.recordPaintStops(result, 'fill', colorKeys);
  return result;
}
