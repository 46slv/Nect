export function evaluateCompositionReference(_operation, read, context) {
  const position = read('position'),
    { reference, appearance } = context.compositionInput(),
    result = context.groupAppearance([appearance], [1, 0, 0, 1, ...position]);
  context.identity.setFingerprint(result, () => [reference, context.nodeId, position, appearance]);
  return result;
}
