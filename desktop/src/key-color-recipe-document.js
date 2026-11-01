import { KEY_COLOR_RECIPE_VERSION, restoreKeyColorRecipe } from './key-color-recipe.js';

// Native retention only. This does not enable generation, live palette ownership,
// a new UI, or any coefficient/alpha/Neutral/Text policy.
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
