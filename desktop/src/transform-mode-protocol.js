// Snapshot only structural presence and routing; parameter values remain host-owned.
function inspect(shape) {
  if (
    !shape ||
    !Object.isFrozen(shape) ||
    typeof shape.nodeId !== 'string' ||
    !shape.nodeId ||
    !['path', 'painted'].includes(shape.mode) ||
    !Array.isArray(shape.slots) ||
    !Object.isFrozen(shape.slots) ||
    shape.slots.length > 32
  )
    throw Error('Invalid Transform snapshot');
  const ids = new Set();
  for (const slot of shape.slots) {
    if (!slot || !Object.isFrozen(slot) || typeof slot.id !== 'string' || ids.has(slot.id))
      throw Error('Invalid Transform snapshot');
    ids.add(slot.id);
  }
  return shape.mode === 'painted' ? 'PaintedPath' : 'Path';
}

export const transformModeProtocol = Object.freeze({
  validateDeclaration: (shape) =>
    !!shape &&
    Object.keys(shape).length === 2 &&
    shape.protocol === 'nect.transform-mode' &&
    shape.revision === 1,
  snapshot(node) {
    if (node.paintMode !== undefined && node.paintMode !== true)
      throw Error('Invalid Transform snapshot');
    const shape = Object.freeze({
      nodeId: node.id,
      mode: node.paintMode ? 'painted' : 'path',
      slots: Object.freeze(Object.keys(node.params).map((id) => Object.freeze({ id }))),
    });
    inspect(shape);
    return shape;
  },
  resolve(shape, outputs) {
    const resultType = inspect(shape);
    return Object.freeze({
      resultType,
      outputs: Object.freeze(
        outputs.map((output) => Object.freeze({ ...output, type: resultType })),
      ),
      active: Object.freeze(shape.slots.map(({ id }) => id)),
    });
  },
  initialFields: () => ({}),
});
