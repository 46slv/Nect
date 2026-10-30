import {
  DEFAULT_HARMONY_WHEEL,
  normalizeHarmonyWheel,
  wheelPoint,
  resolveHarmonyPalette,
} from './harmony-wheel.js';
import { MAX_CHROMA } from './auto-chroma.js';
// Authored shared colors. Identity is independent of the resolved Color value.
export const COLOR_SLOTS_SCHEMA_VERSION = 54;
export const COLOR_DESIGN_SCHEMA_VERSION = 55;
export const HARMONY_DESIGN_SCHEMA_VERSION = 56;
export const CHROMA_OVERRIDE_SCHEMA_VERSION = 57;
function designSchemaVersion(design) {
  if (design?.version !== 2) return COLOR_DESIGN_SCHEMA_VERSION;
  return Array.isArray(design.palette) &&
    design.palette.some((entry) => entry && Object.hasOwn(entry, 'overrideC'))
    ? CHROMA_OVERRIDE_SCHEMA_VERSION
    : HARMONY_DESIGN_SCHEMA_VERSION;
}
export const colorSlotCommands = new Set([
  'create-color-slot',
  'create-harmony-palette',
  'add-harmony-color',
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
export function validateColorDesign(doc, slots) {
  const design = doc.colorDesign;
  if (design === undefined) return;
  const wheelDesign = design?.version === 2,
    hasSpread = Object.hasOwn(design || {}, 'spread');
  const keys = [
    'version',
    'profile',
    'colorfulness',
    'areaAware',
    'lockedSlotIds',
    ...(hasSpread ? ['spread'] : []),
    ...(wheelDesign ? ['wheel', 'palette'] : []),
  ];
  if (
    doc.schemaVersion < designSchemaVersion(design) ||
    !design ||
    typeof design !== 'object' ||
    Array.isArray(design) ||
    Object.keys(design).length !== keys.length ||
    Object.keys(design).some((key) => !keys.includes(key)) ||
    ![1, 2].includes(design.version) ||
    !['balanced', 'fidelity', 'vibrant', 'soft', 'uniform'].includes(design.profile) ||
    !Number.isFinite(design.colorfulness) ||
    design.colorfulness < 0 ||
    design.colorfulness > 1 ||
    (hasSpread &&
      (!Number.isFinite(design.spread) || design.spread < 0 || design.spread > 1)) ||
    typeof design.areaAware !== 'boolean' ||
    !Array.isArray(design.lockedSlotIds) ||
    design.lockedSlotIds.length > 256
  )
    throw Error(
      `Invalid saved color design; requires schemaVersion ${designSchemaVersion(design)}`,
    );
  const seen = new Set();
  for (const id of design.lockedSlotIds) {
    if (typeof id !== 'string' || !slots.has(id) || seen.has(id))
      throw Error('Invalid saved color lock reference');
    seen.add(id);
  }
  if (wheelDesign) {
    normalizeHarmonyWheel(design.wheel);
    if (!Array.isArray(design.palette) || !design.palette.length || design.palette.length > 256)
      throw Error('Invalid harmony palette');
    const paletteIds = new Set();
    for (const entry of design.palette) {
      if (
        !entry ||
        typeof entry !== 'object' ||
        Array.isArray(entry) ||
        !slots.has(entry.id) ||
        paletteIds.has(entry.id) ||
        !['Primary', 'Secondary', 'Accent', 'Background', 'Text'].includes(entry.role) ||
        !['chromatic', 'neutral'].includes(entry.kind)
      )
        throw Error('Invalid harmony palette reference');
      paletteIds.add(entry.id);
      const fields =
        entry.kind === 'neutral'
          ? ['id', 'kind', 'L', 'role']
          : [
              'id',
              'kind',
              'position',
              'ring',
              'role',
              ...(Object.hasOwn(entry, 'overrideC') ? ['overrideC'] : []),
            ];
      if (
        Object.keys(entry).length !== fields.length ||
        Object.keys(entry).some((key) => !fields.includes(key))
      )
        throw Error('Invalid harmony palette fields');
      if (entry.kind === 'neutral') {
        if (!Number.isFinite(entry.L) || entry.L < 0 || entry.L > 1)
          throw Error('Invalid neutral lightness');
      } else {
        wheelPoint(design.wheel, entry.position, entry.ring);
        if (
          Object.hasOwn(entry, 'overrideC') &&
          (!Number.isFinite(entry.overrideC) || entry.overrideC < 0 || entry.overrideC > MAX_CHROMA)
        )
          throw Error('Invalid palette chroma override');
      }
    }
  }
}
export function validateColorSlots(doc) {
  if (doc.colorSlots === undefined) {
    const slots = new Map();
    validateColorDesign(doc, slots);
    return slots;
  }
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
  validateColorDesign(doc, slots);
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
  if (type === 'create-harmony-palette' || type === 'add-harmony-color') {
    const creating = type === 'create-harmony-palette';
    if (
      !payload ||
      typeof payload !== 'object' ||
      Array.isArray(payload) ||
      Object.keys(payload).some((key) => !(creating ? [] : ['name', 'kind']).includes(key))
    )
      throw Error('Invalid harmony command payload');
    if (slots.size + (creating ? 5 : 1) > 256) throw Error('Shared Color slot budget exceeded');
    if (!creating && doc.colorDesign?.version !== 2) throw Error('Create a harmony palette first');
    const design = creating
      ? {
          version: 2,
          profile: 'balanced',
          colorfulness: 0.72,
          areaAware: true,
          lockedSlotIds: [],
          wheel: { ...DEFAULT_HARMONY_WHEEL },
          palette: [],
        }
      : structuredClone(doc.colorDesign);
    const newEntries = [];
    const middle = Math.floor(design.wheel.lightnessSteps / 2);
    if (creating) {
      for (let position = 0; position < 3; position++)
        newEntries.push({
          id: makeId(),
          kind: 'chromatic',
          position,
          ring: Math.min(design.wheel.lightnessSteps - 1, middle + (position === 1 ? 1 : 0)),
          role: ['Primary', 'Secondary', 'Accent'][position],
        });
      newEntries.push(
        { id: makeId(), kind: 'neutral', L: 0.94, role: 'Background' },
        { id: makeId(), kind: 'neutral', L: 0.1, role: 'Text' },
      );
    } else {
      const kind = payload.kind === undefined ? 'chromatic' : payload.kind;
      if (!['chromatic', 'neutral'].includes(kind)) throw Error('Invalid harmony color kind');
      if (payload.name !== undefined) name(payload.name);
      newEntries.push(
        kind === 'neutral'
          ? { id: makeId(), kind, L: 0.5, role: 'Background' }
          : {
              id: makeId(),
              kind,
              position: design.palette.filter((entry) => entry.kind === 'chromatic').length % 3,
              ring: middle,
              role: 'Secondary',
            },
      );
    }
    const generated = resolveHarmonyPalette(design.wheel, newEntries, {
      profile: design.profile,
      colorfulness: design.colorfulness,
      spread: design.spread ?? 0.62,
      areaAware: design.areaAware,
    }).colors;
    const added = generated.map((row, index) => ({
      id: row.id,
      name: creating
        ? ['C1', 'C2', 'C3', 'Background', 'Text'][index]
        : (payload.name ?? `Color ${slots.size + 1}`),
      value: { ...row.value },
    }));
    design.palette.push(...newEntries);
    const colors = [...(doc.colorSlots || []), ...added];
    const nextVersion = Math.max(doc.schemaVersion, HARMONY_DESIGN_SCHEMA_VERSION);
    validateColorSlots({
      ...doc,
      schemaVersion: nextVersion,
      colorSlots: colors,
      colorDesign: design,
    });
    doc.colorSlots = colors;
    doc.colorDesign = design;
    doc.schemaVersion = nextVersion;
    return { slotId: added[0].id, slotIds: added.map((slot) => slot.id) };
  }
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
    let design;
    if (payload.design !== undefined) {
      design = structuredClone(payload.design);
      validateColorDesign(
        {
          ...doc,
          schemaVersion: Math.max(doc.schemaVersion, designSchemaVersion(design)),
          colorDesign: design,
        },
        slots,
      );
    }
    // Validate every target, value and design before touching even the private candidate.
    for (const update of updates) update.slot.value = update.value;
    if (design !== undefined) {
      doc.colorDesign = design;
      doc.schemaVersion = Math.max(doc.schemaVersion, designSchemaVersion(design));
    }
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
