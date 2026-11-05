// Bounded trusted built-in protocol. Snapshot contains no authored values or source IDs.
export const JOIN_SLOT_LIMIT = 1024;
const LIMIT = JOIN_SLOT_LIMIT;
// Shared structural rule for codec and the read-only host snapshot.
export function validJoinSlotOrder(keys, params) {
  return (
    Array.isArray(keys) &&
    keys.length >= 2 &&
    keys.length <= LIMIT &&
    keys[0] === 'a' &&
    keys[1] === 'b' &&
    new Set(keys).size === keys.length &&
    !!params &&
    Object.keys(params).length === keys.length &&
    keys.every(
      (key, index) =>
        typeof key === 'string' &&
        (index < 2 || /^input_[A-Za-z0-9_-]{1,100}$/.test(key)) &&
        Object.hasOwn(params, key),
    )
  );
}
function validateDeclaration(shape) {
  return (
    shape &&
    Object.keys(shape).length === 2 &&
    shape.protocol === 'nect.join-slots' &&
    shape.revision === 1
  );
}
function inspect(shape) {
  if (
    !shape ||
    !Object.isFrozen(shape) ||
    typeof shape.nodeId !== 'string' ||
    !shape.nodeId ||
    !['path', 'painted'].includes(shape.mode) ||
    !Array.isArray(shape.slots) ||
    !Object.isFrozen(shape.slots) ||
    shape.slots.length < 2 ||
    shape.slots.length > LIMIT
  )
    throw Error('Invalid slot snapshot');
  const ids = new Set(),
    type = shape.mode === 'painted' ? 'PaintedPath' : 'Path';
  shape.slots.forEach((slot, i) => {
    if (
      !slot ||
      !Object.isFrozen(slot) ||
      typeof slot.id !== 'string' ||
      (i === 0 && slot.id !== 'a') ||
      (i === 1 && slot.id !== 'b') ||
      (i > 1 && !/^input_[A-Za-z0-9_-]{1,100}$/.test(slot.id)) ||
      ids.has(slot.id) ||
      slot.type !== type ||
      typeof slot.connected !== 'boolean' ||
      typeof slot.locked !== 'boolean'
    )
      throw Error('Invalid slot snapshot');
    ids.add(slot.id);
  });
  return type;
}
function snapshot(node) {
  const keys = node.joinInputOrder ?? ['a', 'b'];
  if (
    !validJoinSlotOrder(keys, node.params) ||
    (node.paintMode !== undefined && node.paintMode !== true)
  )
    throw Error('Invalid slot snapshot');
  const mode = node.paintMode === true ? 'painted' : 'path';
  const slots = keys.map((id) => {
    const q = node.params[id];
    if (!q) throw Error('Invalid slot snapshot');
    return Object.freeze({ id, type: q.type, connected: !!q.source, locked: q.locked });
  });
  const shape = Object.freeze({ nodeId: node.id, mode, slots: Object.freeze(slots) });
  inspect(shape);
  return shape;
}
function resolve(shape, outputs) {
  const resultType = inspect(shape);
  return Object.freeze({
    resultType,
    outputs: Object.freeze(outputs.map((output) => Object.freeze({ ...output, type: resultType }))),
    active: Object.freeze(shape.slots.map(({ id }) => id)),
  });
}
export const joinSlotsProtocol = Object.freeze({
  validateDeclaration,
  snapshot,
  resolve,
  initialFields: () => ({ joinInputOrder: ['a', 'b'] }),
});
