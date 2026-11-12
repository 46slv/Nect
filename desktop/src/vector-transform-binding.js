import { transformValues, transformVector } from './transform-affine.js';

export function evaluateVectorTransform(_operation, read, context) {
  const params = {};
  for (const key of ['anchor', 'position', 'uniformScale', 'scaleWidth', 'skew', 'skewAxis'])
    if (context.hasParameter(key)) params[key] = true;
  return transformVector(transformValues({ params }, read), read('vector'));
}
