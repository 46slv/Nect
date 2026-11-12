import { isPaintedGroup } from './composition-reference.js';

export function evaluateTextToPath(_operation, read, context) {
  const source = read('path');
  if (!source) throw Error('Connect a Path input');
  const result = context.cloneEvaluated(source);
  if (isPaintedGroup(result))
    throw Error('Edit paint in the source Composition, or use Convert to Path');
  return result;
}
