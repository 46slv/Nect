import { isPaintedGroup } from './composition-reference.js';

export function evaluateStroke(_operation, read, context) {
  const source = read('path');
  if (!source) throw Error('Connect a PaintedPath input');
  const result = context.cloneEvaluated(source);
  if (isPaintedGroup(result))
    throw Error('Edit paint in the source Composition, or use Convert to Path');
  const width = read('width');
  if (width < 0 || width > 100000) throw Error('Stroke width must be from 0 to 100000 px');
  const style = context.strokeStyle(),
    miterLimit = context.hasParameter('miterLimit') ? read('miterLimit') : 4,
    dashOffset = context.hasParameter('dashOffset') ? read('dashOffset') : 0;
  if (miterLimit < 1 || miterLimit > 10000) throw Error('Miter limit must be from 1 to 10000');
  if (Math.abs(dashOffset) > 100000) throw Error('Dash offset must be from -100000 to 100000 px');
  result.stroke = { ...read('color'), width, ...style, miterLimit, dashOffset };
  context.recordPaintSlot(result, 'stroke', 'color');
  return result;
}
