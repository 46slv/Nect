import { booleanOperand } from './boolean-operand.js';
import { fromPaper } from './paper-runtime.js';
import { routeCharacters } from './character-geometry.js';
import { signature } from './path-signature.js';

export function evaluateBoolean(operation, read, context) {
  const inputs = context.shape.slots
      .filter((slot) => slot.connected)
      .map((slot) => [slot.id, read(slot.id)]),
    a = inputs.find(([key]) => key === 'a')?.[1],
    b = inputs.find(([key]) => key === 'b')?.[1];
  if (!a || !b) throw Error('Connect both Boolean paths');
  if (inputs.some(([, path]) => path.contours.some((contour) => !contour.closed)))
    throw Error('Boolean requires closed contours');
  const op = {
    union: 'unite',
    subtract: 'subtract',
    intersect: 'intersect',
    difference: 'exclude',
  }[operation];
  if (!op) throw Error('Unknown Boolean operation');
  const liveOperands = new Set();
  try {
    let combined;
    for (const [, path] of inputs) {
      const next = booleanOperand(path);
      liveOperands.add(next);
      if (!combined) {
        combined = next;
        continue;
      }
      const previous = combined;
      combined = previous[op](next, { insert: false });
      liveOperands.add(combined);
      previous.remove();
      liveOperands.delete(previous);
      next.remove();
      liveOperands.delete(next);
    }
    const result = fromPaper(combined, context.nodeId);
    result.characterOccurrences = inputs.flatMap(([key, path]) =>
      routeCharacters(
        path,
        context.nodeId + ':' + key,
        null,
        'Canvas character editing is unavailable through Boolean',
      ),
    );
    result.unstable = true;
    result.fingerprint = signature(result);
    return result;
  } finally {
    for (const item of liveOperands) item.remove();
  }
}
