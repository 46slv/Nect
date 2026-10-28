// Authored shared colors. Identity is independent of the resolved Color value.
export const COLOR_SLOTS_SCHEMA_VERSION = 54;
export const colorSlotCommands = new Set([
  'create-color-slot',
  'rename-color-slot',
  'set-color-slot',
  'set-color-slots',
  'bind-color-slot',
  'unbind-color-slot',
]);
const idPattern = /^[A-Za-z0-9_-]{1,128}$/;
function color(value) {
  if (
    !value ||
    typeof value !== 'object' ||
    Array.isArray(value) ||
    Object.keys(value).length !== 2 ||
    typeof value.hex !== 'string' ||
    !/^#[0-9a-fA-F]{6}$/.test(value.hex) ||
    !Number.isFinite(value.alpha) ||
    value.alpha < 0 ||
    value.alpha > 1
  )
    throw Error('Expected literal shared Color {hex, alpha}');
}
function name(value) {
  if (typeof value !== 'string' || !value.trim() || value.length > 128)
    throw Error('Invalid Color slot name');
}
export function validateColorSlots(doc) {
  if (doc.colorSlots === undefined) return new Map();
  if (
    doc.schemaVersion < COLOR_SLOTS_SCHEMA_VERSION ||
    !Array.isArray(doc.colorSlots) ||
    doc.colorSlots.length > 256
  )
    throw Error('Shared colors require schemaVersion 54 and at most 256 slots');
  const slots = new Map();
  for (const slot of doc.colorSlots) {
    if (!slot || typeof slot.id !== 'string' || !idPattern.test(slot.id) || slots.has(slot.id))
      throw Error('Invalid or duplicate Color slot ID');
    name(slot.name);
    color(slot.value);
    slots.set(slot.id, slot);
  }
  return slots;
}
export function validateColorSlotBinding(q, slots) {
  if (q?.colorSlotId === undefined) return;
  if (
    q.type !== 'Color' ||
    typeof q.colorSlotId !== 'string' ||
    !slots.has(q.colorSlotId) ||
    q.source ||
    q.expression
  )
    throw Error('Invalid, dangling or conflicting Color slot binding');
}
export function resolveColorSlot(doc, id) {
  const slot = doc?.colorSlots?.find((slot) => slot.id === id);
  if (!slot) throw Error('Missing shared Color slot');
  color(slot.value);
  return { ...slot.value };
}
export function applyColorSlotCommand(doc, type, payload, makeId) {
  const slots = validateColorSlots(doc);
  if (type === 'set-color-slots') {
    if (!Array.isArray(payload.updates) || payload.updates.length > 256)
      throw Error('Expected a bounded shared Color update list');
    const seen = new Set();
    const updates = Array.from(payload.updates, (update) => {
      if (
        !update ||
        typeof update.slotId !== 'string' ||
        seen.has(update.slotId) ||
        !slots.has(update.slotId)
      )
        throw Error('Missing or duplicate shared Color update target');
      seen.add(update.slotId);
      color(update.value);
      return { slot: slots.get(update.slotId), value: { ...update.value } };
    });
    // Validate every target and value before touching even the private candidate.
    for (const update of updates) update.slot.value = update.value;
    return { slotIds: [...seen] };
  }
  if (type === 'create-color-slot') {
    const id = makeId();
    name(payload.name);
    color(payload.value);
    doc.colorSlots ??= [];
    doc.colorSlots.push({ id, name: payload.name, value: { ...payload.value } });
    doc.schemaVersion = Math.max(doc.schemaVersion, COLOR_SLOTS_SCHEMA_VERSION);
    validateColorSlots(doc);
    return { slotId: id };
  }
  if (type === 'rename-color-slot' || type === 'set-color-slot') {
    const slot = slots.get(payload.slotId);
    if (!slot) throw Error('Missing shared Color slot');
    if (type === 'rename-color-slot') {
      name(payload.name);
      slot.name = payload.name;
    } else {
      color(payload.value);
      slot.value = { ...payload.value };
    }
    return { slotId: slot.id };
  }
  const layer = doc.layers.find((l) => l.id === payload.layerId);
  const node = layer?.nodes.find((n) => n.id === payload.nodeId);
  const q = node?.params?.[payload.key];
  if (!q || q.type !== 'Color' || layer.locked || q.locked || q.source || q.expression)
    throw Error('Select an unlocked, undriven Color parameter');
  if (type === 'bind-color-slot') {
    if (!slots.has(payload.slotId)) throw Error('Missing shared Color slot');
    q.colorSlotId = payload.slotId;
  } else if (type === 'unbind-color-slot') {
    if (q.colorSlotId !== undefined) q.value = resolveColorSlot(doc, q.colorSlotId);
    delete q.colorSlotId;
  } else throw Error('Unknown shared Color command');
  return {};
}

// Import only referenced slots; never coalesce independent identities by value.
// `copies` are already detached/remapped geometry. Neither input document mutates.
export function importColorSlots(current, source, copies, occupied) {
  const sourceSlots = validateColorSlots(source),
    refs = new Set();
  function walk(value, visit) {
    if (!value || typeof value !== 'object') return;
    if (Array.isArray(value)) {
      for (const item of value) walk(item, visit);
      return;
    }
    if (value.colorSlotId !== undefined) visit(value);
    for (const child of Object.values(value)) walk(child, visit);
  }
  walk(copies, (q) => refs.add(q.colorSlotId));
  const result = structuredClone(current.colorSlots || []),
    mapping = new Map();
  for (const id of refs) {
    const slot = sourceSlots.get(id);
    if (!slot) throw Error('Missing imported Color slot');
    let next = id,
      index = 1;
    while (occupied.has(next)) next = `${id.slice(0, 96)}-import-${index++}`;
    occupied.add(next);
    mapping.set(id, next);
    result.push({ ...structuredClone(slot), id: next });
  }
  walk(copies, (q) => {
    q.colorSlotId = mapping.get(q.colorSlotId);
  });
  if (result.length > 256) throw Error('Imported Color slot budget exceeded');
  return current.colorSlots !== undefined || refs.size ? result : undefined;
}
