import { MAX_COMPOSITION_DIMENSION } from './composition-project-model.js';

export function isNewProjectDimension(value) {
  return Number.isFinite(value) && value > 0 && value <= MAX_COMPOSITION_DIMENSION;
}
