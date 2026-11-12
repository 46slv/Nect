import { radialPath } from './radial-geometry.js';

export const evaluatePolygon = (_operation, read, context) =>
  radialPath('Polygon', context.nodeId, read);
