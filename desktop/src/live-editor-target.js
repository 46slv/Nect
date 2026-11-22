const keys = [
  'documentGeneration',
  'compositionId',
  'sessionId',
  'sessionRevision',
  'projectRevision',
];

export function captureEditorTarget(values) {
  return Object.freeze(Object.fromEntries(keys.map((key) => [key, values[key]])));
}

export function validEditorTarget(target) {
  const id = (value) => typeof value === 'string' && value.length > 0 && value.length <= 256;
  const version = (value) => Number.isSafeInteger(value) && value >= 0;
  return (
    target !== null &&
    typeof target === 'object' &&
    Object.getPrototypeOf(target) === Object.prototype &&
    Object.keys(target).length === keys.length &&
    keys.every((key) => Object.hasOwn(target, key)) &&
    version(target.documentGeneration) &&
    id(target.compositionId) &&
    id(target.sessionId) &&
    version(target.sessionRevision) &&
    version(target.projectRevision)
  );
}

// Read-only projections do not need the edit command dispatcher.
export function readEditorInspection(readTarget, canInspect = () => true) {
  try {
    if (canInspect()) {
      const target = captureEditorTarget(readTarget());
      if (validEditorTarget(target)) return { status: 'ok', target };
    }
  } catch {
    // A transitioning host is indistinguishable from an unavailable document.
  }
  return { status: 'unavailable' };
}

export function editorTargetMatches(current, expected) {
  return !!expected && keys.every((key) => current[key] === expected[key]);
}
