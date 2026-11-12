import { groupGeometry, isPaintedGroup } from './composition-reference.js';
import { signature } from './path-signature.js';

export function evaluateConvertToPath(_operation, read, context) {
  const source = read('path');
  if (!source) throw Error('Connect a Path or PaintedPath input');
  const result = isPaintedGroup(source) ? groupGeometry(source) : context.cloneEvaluated(source);
  if (context.sourceOutputType('path') === 'PaintedPath') {
    delete result.fill;
    delete result.stroke;
    context.identity.setFingerprint(result, (currentEra) => [
      'painted-path-geometry',
      context.identity.provenance(currentEra),
      signature(result),
      result.contours.map((contour) => [contour.id, contour.points.map((point) => point.id)]),
    ]);
  }
  return result;
}
