import {
  KEY_COLOR_RECIPE_VERSION,
  KEY_COLOR_SOLVER_VERSION,
  KEY_COLOR_SEMANTICS_VERSION,
  createKeyColorRecipe,
  restoreKeyColorRecipe,
} from './key-color-recipe.js';

// Native receipts remain historical, never live palette ownership. An explicit
// one-shot application may materialize values without enabling a UI or defaults.
export const KEY_COLOR_RECIPE_DOCUMENT_SCHEMA_VERSION = 58;
export const keyColorRecipeDocumentCommands = new Set([
  'retain-key-color-recipe-snapshot',
  'replace-key-color-recipe-snapshot',
  'clear-key-color-recipe-snapshot',
]);
// Native receipts must be lossless JSON data before cloning or prototype
// normalization can omit unsupported values. This is intentionally stricter than
// the pure model: it also rejects array properties JSON.stringify would drop.
function assertJsonSnapshot(value, path = '$.committedSnapshot', ancestors = new Set()) {
  if (value === null || typeof value === 'string' || typeof value === 'boolean') return;
  if (typeof value === 'number') {
    if (!Number.isFinite(value)) throw Error(`Invalid retained JSON number at ${path}`);
    return;
  }
  if (!value || typeof value !== 'object') throw Error(`Invalid retained JSON value at ${path}`);
  if (ancestors.has(value)) throw Error(`Cyclic retained JSON value at ${path}`);
  const array = Array.isArray(value);
  if (array && Object.getPrototypeOf(value) !== Array.prototype)
    throw Error(`Invalid retained JSON array at ${path}`);
  if (!array && ![Object.prototype, null].includes(Object.getPrototypeOf(value)))
    throw Error(`Invalid retained JSON object at ${path}`);
  if (Object.getOwnPropertySymbols(value).length)
    throw Error(`Symbol keys in retained JSON at ${path}`);
  for (const key of Object.getOwnPropertyNames(value)) {
    if (array && key === 'length') continue;
    const descriptor = Object.getOwnPropertyDescriptor(value, key);
    if (!descriptor.enumerable || !Object.hasOwn(descriptor, 'value'))
      throw Error(`Unsupported retained JSON property at ${path}.${key}`);
  }
  ancestors.add(value);
  if (array) {
    for (let i = 0; i < value.length; i++) {
      if (!Object.hasOwn(value, i)) throw Error(`Sparse retained JSON array at ${path}[${i}]`);
      assertJsonSnapshot(value[i], `${path}[${i}]`, ancestors);
    }
    if (Object.keys(value).length !== value.length)
      throw Error(`Extra retained JSON array property at ${path}`);
  } else {
    for (const [key, child] of Object.entries(value))
      assertJsonSnapshot(child, `${path}.${key}`, ancestors);
  }
  ancestors.delete(value);
}
function fields(value, keys) {
  if (
    !value ||
    typeof value !== 'object' ||
    Array.isArray(value) ||
    ![Object.prototype, null].includes(Object.getPrototypeOf(value)) ||
    Object.getOwnPropertySymbols(value).length !== 0 ||
    Object.getOwnPropertyNames(value).length !== keys.length ||
    keys.some((key) => {
      const d = Object.getOwnPropertyDescriptor(value, key);
      return d && (!d.enumerable || !Object.hasOwn(d, 'value'));
    }) ||
    keys.some((key) => !Object.hasOwn(value, key))
  )
    throw Error('Invalid retained key-color recipe fields');
}
function orderedJson(value) {
  if (Array.isArray(value)) return value.map(orderedJson);
  if (value && typeof value === 'object')
    return Object.fromEntries(
      Object.keys(value)
        .sort()
        .map((key) => [key, orderedJson(value[key])]),
    );
  return value;
}
function snapshotWithExistingSlots(value, slots) {
  assertJsonSnapshot(value);
  const snapshot = restoreKeyColorRecipe(value);
  // The receipt holds references, not registry ownership or current-value/name
  // equality. Inactive/dormant IDs are deliberately included in this check.
  for (const row of snapshot.resolved)
    if (!slots.has(row.id)) throw Error('Missing retained key-color slot reference');
  return snapshot;
}
// Developer/test-fixture boundary only: no product plan, coefficients, roles,
// generated count, names or allocation are selected here. Validate raw data
// before the pure model's structuredClone can discard unsupported properties.
function initializationRequest(raw) {
  assertJsonSnapshot(raw, '$.initialization');
  // Detach the entire guarded request, not just model intent. In particular,
  // allocations must never execute caller-supplied inherited/proxy methods.
  const payload = structuredClone(raw);
  fields(payload, [
    'version',
    'modelVersion',
    'solverVersion',
    'colorSemanticsVersion',
    'paletteId',
    'intent',
    'allocations',
  ]);
  if (
    payload.version !== 1 ||
    payload.modelVersion !== KEY_COLOR_RECIPE_VERSION ||
    payload.solverVersion !== KEY_COLOR_SOLVER_VERSION ||
    payload.colorSemanticsVersion !== KEY_COLOR_SEMANTICS_VERSION
  )
    throw Error('Unsupported key-color initialization version');
  const { intent } = payload;
  fields(intent, ['keyColor', 'preset', 'slots', 'settings']);
  fields(intent.keyColor, ['id', 'role', 'value', 'hueHint']);
  fields(intent.keyColor.value, ['hex', 'alpha']);
  fields(intent.settings, ['areaAware']);
  fields(intent.preset, ['id', 'version', 'harmony', 'hueSteps', 'slots']);
  if (intent.preset.version !== 1) throw Error('Unsupported initialization preset version');
  if (
    !Array.isArray(intent.slots) ||
    intent.slots.length > 255 ||
    !Array.isArray(intent.preset.slots) ||
    intent.preset.slots.length > 255 ||
    !Array.isArray(payload.allocations) ||
    payload.allocations.length > 256
  )
    throw Error('Invalid key-color initialization lists');
  for (const slot of intent.slots) fields(slot, ['id', 'role', 'deltaL', 'deltaRatioCmax']);
  for (const plan of intent.preset.slots)
    fields(plan, [
      'id',
      'role',
      'position',
      'lightnessScale',
      'lightnessOffset',
      'chromaScale',
      'chromaOffset',
      'alpha',
    ]);
  const allocated = new Set();
  for (const allocation of payload.allocations) {
    fields(allocation, ['slotId', 'name']);
    if (
      typeof allocation.slotId !== 'string' ||
      !/^[A-Za-z0-9_-]{1,128}$/.test(allocation.slotId) ||
      allocated.has(allocation.slotId)
    )
      throw Error('Invalid or duplicate key-color allocation ID');
    if (
      typeof allocation.name !== 'string' ||
      !allocation.name.trim() ||
      allocation.name.length > 128
    )
      throw Error('Invalid Color slot name');
    allocated.add(allocation.slotId);
  }
  const intended = new Set([intent.keyColor.id, ...intent.slots.map((slot) => slot.id)]);
  if (allocated.size !== intended.size || [...intended].some((id) => !allocated.has(id)))
    throw Error('Key-color allocations must exactly cover explicit intent IDs');
  return payload;
}
export function keyColorRecipeInitializationSlotIds(payload) {
  return initializationRequest(payload).allocations.map((allocation) => allocation.slotId);
}
export function prepareKeyColorRecipeInitialization(doc, payload, slots) {
  const request = initializationRequest(payload),
    slotIds = request.allocations.map((allocation) => allocation.slotId);
  if (Object.hasOwn(doc, 'keyColorRecipe'))
    throw Error('Retained key-color recipe exists; initialization is create-only');
  if (slots.size + slotIds.length > 256) throw Error('Shared Color slot budget exceeded');
  if (slotIds.some((id) => slots.has(id)))
    throw Error('Key-color initialization requires fresh slot IDs');
  // Fresh slots have no historic colors. An omitted derived mapping must reject,
  // never become invented dormant history. The pure create path enforces this.
  const snapshot = createKeyColorRecipe({ paletteId: request.paletteId, ...request.intent }),
    proposed = new Map(snapshot.resolved.map((row) => [row.id, row]));
  const prepared = prepareKeyColorRecipeApplication(doc, { snapshot }, proposed);
  return {
    snapshot: prepared.snapshot,
    slots: request.allocations.map(({ slotId, name }) => ({
      id: slotId,
      name,
      value: { ...proposed.get(slotId).value },
    })),
  };
}
export function prepareKeyColorRecipeApplication(doc, payload, slots) {
  fields(payload, ['snapshot']);
  const snapshot = snapshotWithExistingSlots(payload.snapshot, slots),
    active = snapshot.resolved.filter((row) => row.active),
    locked = new Set(doc.colorDesign?.lockedSlotIds || []),
    keyId = snapshot.intent.keyColor.id;
  // Legacy generator locks are not silently taken over, even if a target's
  // current literal already matches the receipt. Dormant rows are not targets.
  if (active.some((row) => locked.has(row.id)))
    throw Error('KEY_COLOR_TARGET_LOCKED: unlock active shared Color targets before Apply');
  // Retention still accepts old experimental alpha intent. New application is
  // narrower: C1 preserves its exact literal alpha; active derived colors are
  // opaque. Reject incompatible receipts instead of rewriting their samples.
  if (
    active.some((row) => row.id !== keyId && row.value.alpha !== 1) ||
    snapshot.intent.preset.slots.some((plan) => plan.alpha !== undefined && plan.alpha !== 1)
  )
    throw Error('KEY_COLOR_DERIVED_ALPHA: active derived colors must be opaque for Apply');
  return {
    snapshot,
    updates: active.map((row) => ({ slotId: row.id, value: { ...row.value } })),
  };
}
export function validateRetainedKeyColorRecipe(doc, slots) {
  if (
    Array.isArray(doc.compositions) &&
    doc.compositions.some((composition) => Object.hasOwn(composition || {}, 'keyColorRecipe'))
  )
    throw Error('Retained key-color recipe belongs at the project root only');
  if (!Object.hasOwn(doc, 'keyColorRecipe')) return;
  if (
    !Number.isInteger(doc.schemaVersion) ||
    doc.schemaVersion < KEY_COLOR_RECIPE_DOCUMENT_SCHEMA_VERSION
  )
    throw Error('Retained key-color recipe requires schemaVersion 58');
  const packet = doc.keyColorRecipe;
  fields(packet, ['version', 'mode', 'committedSnapshot']);
  if (packet.version !== 1 || packet.mode !== 'retained-only')
    throw Error('Unsupported retained key-color recipe version or mode');
  if (packet.committedSnapshot?.version !== KEY_COLOR_RECIPE_VERSION)
    throw Error('Native retained key-color recipe requires canonical prototype snapshot v2');
  return snapshotWithExistingSlots(packet.committedSnapshot, slots);
}
export function applyRetainedKeyColorRecipeCommand(doc, type, payload, slots) {
  const present = Object.hasOwn(doc, 'keyColorRecipe');
  if (type === 'clear-key-color-recipe-snapshot') {
    fields(payload, []);
    delete doc.keyColorRecipe;
    // Do not downgrade schema or delete any colors/references on clear.
    return {};
  }
  if (!keyColorRecipeDocumentCommands.has(type)) throw Error('Unknown retained key-color command');
  fields(payload, ['snapshot']);
  if (type === 'retain-key-color-recipe-snapshot' && present)
    throw Error('Retained key-color recipe exists; use explicit replace or clear');
  if (type === 'replace-key-color-recipe-snapshot' && !present)
    throw Error('No retained key-color recipe; use explicit retain');
  // Supported old prototype inputs normalize metadata only on this explicit
  // command. Native writes always use the current snapshot format.
  const snapshot = snapshotWithExistingSlots(payload.snapshot, slots),
    packet = { version: 1, mode: 'retained-only', committedSnapshot: snapshot },
    schemaVersion = Math.max(doc.schemaVersion, KEY_COLOR_RECIPE_DOCUMENT_SCHEMA_VERSION);
  validateRetainedKeyColorRecipe({ ...doc, schemaVersion, keyColorRecipe: packet }, slots);
  const result = { paletteId: snapshot.paletteId, paletteRevision: snapshot.revision };
  // Object key order is not authored snapshot meaning. Preserve the stored bytes
  // and pending Redo for equivalent v1-normalized or differently ordered input.
  if (
    present &&
    JSON.stringify(orderedJson(doc.keyColorRecipe)) === JSON.stringify(orderedJson(packet))
  )
    return result;
  doc.keyColorRecipe = packet;
  doc.schemaVersion = schemaVersion;
  return result;
}
