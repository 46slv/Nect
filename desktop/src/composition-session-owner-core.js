import {
  CommandSession,
  History,
  blankDocument,
  cloneDocument,
  sameDocument,
  uid,
  validate,
  bindProjectDocumentContext,
  normalizedProjectSourceForDocument,
  assertProjectReferenceTransition,
  artworkClipboardTargetsIntact,
} from './core.js';
import { CompositionSessionOwner } from './composition-session-owner.js';
import { MACRO_SCHEMA_VERSION } from './curve-point-reference.js';

export function createCompositionSessionOwner(project, options = {}) {
  const version =
    project && typeof project === 'object'
      ? Object.getOwnPropertyDescriptor(project, 'schemaVersion')
      : null;
  if (version && !Object.hasOwn(version, 'value')) throw Error('Invalid schema version descriptor');
  return new CompositionSessionOwner(project, {
    ...options,
    macroProjectAdmission:
      options.macroProjectAdmission ??
      (version?.value >= MACRO_SCHEMA_VERSION
        ? (schemaVersion) => schemaVersion >= MACRO_SCHEMA_VERSION
        : null),
    History,
    CommandSession,
    blankDocument,
    cloneDocument,
    sameDocument,
    uid,
    validateDocument: validate,
    bindProjectDocumentContext,
    normalizedProjectSourceForDocument,
    assertProjectReferenceTransition,
    hasClipboardArtworkTargets: artworkClipboardTargetsIntact,
  });
}
