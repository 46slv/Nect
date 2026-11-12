import { isPaintedGroup } from './composition-reference.js';

export function evaluateFill(_operation, read, context) {
  const source = read('path');
  if (!source) throw Error('Connect a PaintedPath input');
  const result = context.cloneEvaluated(source);
  if (isPaintedGroup(result))
    throw Error('Edit paint in the source Composition, or use Convert to Path');
  result.fill = read('color');
  context.recordPaintSlot(result, 'fill', 'color');
  return result;
}
