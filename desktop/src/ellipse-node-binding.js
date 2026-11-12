import { primitiveGeometry } from './primitive-geometry.js';

export const evaluateEllipse = (_operation, read, context) =>
  primitiveGeometry('Ellipse', context.nodeId, read);
