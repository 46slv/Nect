import { isPaintedGroup } from './composition-reference.js';
import { ANGULAR_KEYS, createAngularGradientFill } from './angular-gradient.js';

const colorKeys = Object.freeze(['startColor', 'color1', 'color2', 'endColor']);

export function angularGradientSettings(read) {
  return createAngularGradientFill(Object.fromEntries(ANGULAR_KEYS.map((key) => [key, read(key)])));
}

export function evaluateAngularGradient(_operation, read, context) {
  const source = read('path');
  if (!source) throw Error('Connect a PaintedPath input');
  const result = context.cloneEvaluated(source);
  if (isPaintedGroup(result))
    throw Error('Edit paint in the source Composition, or use Convert to Path');
  result.fill = angularGradientSettings(read);
  context.recordPaintStops(result, 'fill', colorKeys);
  return result;
}
