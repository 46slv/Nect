import { primitiveGeometry } from './primitive-geometry.js';

export const evaluateCircle = (_operation, read, context) =>
  primitiveGeometry('Circle', context.nodeId, read);
