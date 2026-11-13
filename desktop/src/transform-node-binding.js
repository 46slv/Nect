import { fromPaper, getPaperScope, toPaper } from './paper-runtime.js';
import { isPaintedGroup } from './composition-reference.js';
import { transformValues } from './transform-affine.js';
import { routeCharacters } from './character-geometry.js';

export function evaluateTransform(_operation, read, context) {
  const source = read('path');
  if (!source)
    throw Error(`Connect a ${context.shape.mode === 'painted' ? 'PaintedPath' : 'Path'} input`);
  const node = { params: Object.fromEntries(context.shape.slots.map(({ id }) => [id, true])) };
  if (context.shape.mode === 'painted') {
    const result = context.groupAppearance([source], transformValues(node, read).matrix);
    context.identity.setFingerprint(result, () => [context.nodeId, source, result.matrix]);
    return result;
  }
  const input = context.cloneEvaluated(source);
  if (isPaintedGroup(input))
    throw Error('Edit paint in the source Composition, or use Convert to Path');
  const obj = toPaper(input),
    state = transformValues(node, read);
  obj.transform(new (getPaperScope().Matrix)(...state.matrix));
  const transformed = fromPaper(obj, context.nodeId);
  transformed.contours.forEach((contour, ci) => {
    contour.id = input.contours[ci].id;
    if (input.contours[ci].group !== undefined) contour.group = input.contours[ci].group;
    if (input.contours[ci].repeaterOpacity !== undefined)
      contour.repeaterOpacity = input.contours[ci].repeaterOpacity;
    if (input.contours[ci].repeaterFill !== undefined)
      contour.repeaterFill = structuredClone(input.contours[ci].repeaterFill);
    if (input.contours[ci].repeaterStroke !== undefined)
      contour.repeaterStroke = structuredClone(input.contours[ci].repeaterStroke);
    contour.points.forEach((point, pi) => (point.id = input.contours[ci].points[pi].id));
  });
  transformed.characterOccurrences = routeCharacters(input, context.nodeId, state.matrix);
  transformed.unstable = input.unstable;
  transformed.remapSuspended = input.remapSuspended;
  transformed.fingerprint = input.fingerprint;
  context.identity.copy(input, transformed);
  obj.remove();
  return transformed;
}
