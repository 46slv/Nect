import { maskAppearance } from './appearance-mask.js';

export function evaluateMask(_operation, read, context) {
  const source = read('path'),
    shape = read('mask');
  if (!source || !shape) throw Error('Connect appearance and closed Path or Image to Mask');
  const result = maskAppearance(source, shape, {
    invert: read('invert'),
    feather: read('feather'),
    luminance: read('luminance'),
    prefix: context.nodeId,
    bounds: context.pathBounds(source),
  });
  context.identity.copy(source, result.children[0]);
  return result;
}
