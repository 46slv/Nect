import { radialPath } from './radial-geometry.js';

export const evaluateStar = (_operation, read, context) => radialPath('Star', context.nodeId, read);
