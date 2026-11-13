import { isPaintedGroup } from './composition-reference.js';
import { createRadialGradientFill } from './radial-gradient.js';

const keys = ['center', 'radius', 'startColor', 'endColor'];
const colorKeys = Object.freeze(['startColor', 'endColor']);

export function radialGradientSettings(read) {
  return createRadialGradientFill(Object.fromEntries(keys.map((key) => [key, read(key)])));
}

export function evaluateRadialGradient(_operation, read, context) {
  const source = read('path');
  if (!source) throw Error('Connect a PaintedPath input');
  const result = context.cloneEvaluated(source);
  if (isPaintedGroup(result))
    throw Error('Edit paint in the source Composition, or use Convert to Path');
  result.fill = radialGradientSettings(read);
  context.recordPaintStops(result, 'fill', colorKeys);
  return result;
}
