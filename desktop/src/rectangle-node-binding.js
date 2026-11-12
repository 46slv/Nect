import { primitiveGeometry } from './primitive-geometry.js';

export const evaluateRectangle = (_operation, read, context) =>
  primitiveGeometry('Rectangle', context.nodeId, read);
