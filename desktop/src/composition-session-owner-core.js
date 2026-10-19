import {
  CommandSession,
  History,
  blankDocument,
  cloneDocument,
  sameDocument,
  uid,
  validate,
} from './core.js';
import { CompositionSessionOwner } from './composition-session-owner.js';

export function createCompositionSessionOwner(project, options = {}) {
  return new CompositionSessionOwner(project, {
    ...options,
    History,
    CommandSession,
    blankDocument,
    cloneDocument,
    sameDocument,
    uid,
    validateDocument: validate,
  });
}
