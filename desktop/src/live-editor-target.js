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

export function editorTargetMatches(current, expected) {
  return !!expected && keys.every((key) => current[key] === expected[key]);
}
