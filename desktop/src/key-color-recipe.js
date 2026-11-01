import { MAX_CHROMA, proposeAutoChroma } from './auto-chroma.js';
import { harmonyCells } from './harmony-wheel.js';

// Isolated prototype: not a native schema, Session command or public editor API.
export const KEY_COLOR_RECIPE_VERSION = 2;
export const KEY_COLOR_SOLVER_VERSION = 'experimental-affine-v1';
export const KEY_COLOR_SEMANTICS_VERSION = 'linear-srgb-straight-v1';
const clamp = (n) => Math.max(0, Math.min(1, n));
const norm = (h) => ((h % 360) + 360) % 360;
const unit = (n) => Number.isFinite(n) && n >= 0 && n <= 1;
const idPattern = /^[A-Za-z0-9_-]{1,128}$/;
function fields(value, required, optional = []) {
  if (
    !value ||
    typeof value !== 'object' ||
    Array.isArray(value) ||
    required.some((key) => !Object.hasOwn(value, key)) ||
    Object.keys(value).some((key) => ![...required, ...optional].includes(key))
  )
    throw Error('Invalid key-color recipe fields');
}
function id(value) {
  if (typeof value !== 'string' || !idPattern.test(value)) throw Error('Invalid stable ID');
}
function role(value) {
  if (typeof value !== 'string' || !value.trim() || value.length > 128)
    throw Error('Invalid slot role');
}
function literal(value) {
  fields(value, ['hex', 'alpha']);
  if (typeof value.hex !== 'string' || !/^#[a-f0-9]{6}$/i.test(value.hex) || !unit(value.alpha))
    throw Error('Expected sRGB Color {hex, alpha}');
}
function list(value, maximum = 256) {
  if (!Array.isArray(value) || value.length > maximum) throw Error('Invalid recipe list');
  return Array.from(value);
}
function finite(...values) {
  if (!values.every(Number.isFinite)) throw Error('Nonfinite recipe calculation');
}
const decode = (value) => (value <= 0.04045 ? value / 12.92 : ((value + 0.055) / 1.055) ** 2.4);
const hexLinear = (hex) => [1, 3, 5].map((i) => decode(parseInt(hex.slice(i, i + 2), 16) / 255));

// Same OKLab matrices as auto-chroma.js; kept private rather than widening its API.
function linearFromLch(L, C, hue) {
  const angle = (hue * Math.PI) / 180,
    a = C * Math.cos(angle),
    b = C * Math.sin(angle),
    l = (L + 0.3963377774 * a + 0.2158037573 * b) ** 3,
    m = (L - 0.1055613458 * a - 0.0638541728 * b) ** 3,
    s = (L - 0.0894841775 * a - 1.291485548 * b) ** 3;
  const rgb = [
    4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
    -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
    -0.0041960863 * l - 0.7034186147 * m + 1.707614701 * s,
  ];
  // The gamut solver locates polynomial boundaries; direct reconstruction can
  // differ by a few ulps at that boundary (notably the disconnected blue cusp).
  const tolerance = 1e-7 + 8 * Number.EPSILON;
  if (rgb.some((n) => !Number.isFinite(n) || n < -tolerance || n > 1 + tolerance))
    throw Error('Resolved color outside sRGB gamut');
  return rgb.map(clamp); // Only the existing gamut engine's numerical tolerance.
}
function gamut(L, h, C = MAX_CHROMA, alpha = 1) {
  return proposeAutoChroma([{ id: 'sample', role: 'Primary', L, h, overrideC: C, alpha }], {
    areaAware: false,
  })[0];
}
// Target is after the 0..1 ratio clamp, before gamut fallback. Realized is
// measured from the committed C. Keep the old name as a deprecated target alias.
function ratioFields(C, cmax, rTarget) {
  return { rTarget, rRealized: cmax === 0 ? 0 : C / cmax, effectiveRatioCmax: rTarget };
}
function keySample(key) {
  const [r, g, b] = hexLinear(key.value.hex);
  let L, C, h;
  if (r === g && g === b) {
    // Exact encoded neutrals must not acquire hue from matrix roundoff.
    L = Math.cbrt(r);
    C = 0;
    h = key.hueHint;
  } else {
    const l = Math.cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b),
      m = Math.cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b),
      s = Math.cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b),
      a = 1.9779984951 * l - 2.428592205 * m + 0.4505937099 * s,
      labB = 0.0259040371 * l + 0.7827717662 * m - 0.808675766 * s;
    L = clamp(0.2104542553 * l + 0.793617785 * m - 0.0040720468 * s);
    C = Math.hypot(a, labB);
    h = norm((Math.atan2(labB, a) * 180) / Math.PI);
  }
  const cmax = gamut(L, h).cmax;
  return {
    id: key.id,
    role: key.role,
    active: true,
    value: { ...key.value }, // C1 never passes through an OKLCH/HEX round trip.
    linear: [r, g, b, key.value.alpha],
    L,
    C,
    h,
    cmax,
    ...ratioFields(C, cmax, cmax === 0 ? 0 : clamp(C / cmax)),
    diagnostics: [
      ...(C === 0 ? ['achromatic-hue-hint'] : []),
      ...(cmax === 0 ? ['zero-cmax'] : []),
    ],
  };
}
function cells(preset) {
  return harmonyCells({
    harmony: preset.harmony,
    hueSteps: preset.hueSteps,
    hueRotation: 0,
    lightnessSteps: 1,
    lightnessRange: 0,
  });
}
function validateIntent(intent) {
  fields(intent, ['keyColor', 'preset', 'slots', 'settings']);
  const { keyColor: key, preset } = intent;
  fields(key, ['id', 'role', 'value', 'hueHint']);
  id(key.id);
  role(key.role);
  literal(key.value);
  if (!Number.isFinite(key.hueHint) || key.hueHint < 0 || key.hueHint >= 360)
    throw Error('Invalid achromatic hue hint');
  fields(intent.settings, ['areaAware']);
  if (typeof intent.settings.areaAware !== 'boolean') throw Error('Invalid area setting');
  const slots = new Map([[key.id, key]]);
  for (const slot of list(intent.slots, 255)) {
    fields(slot, ['id', 'role', 'deltaL', 'deltaRatioCmax']);
    id(slot.id);
    role(slot.role);
    finite(slot.deltaL, slot.deltaRatioCmax);
    if (slots.has(slot.id)) throw Error('Duplicate stable slot ID');
    slots.set(slot.id, slot);
  }
  fields(preset, ['id', 'version', 'harmony', 'hueSteps', 'slots']);
  id(preset.id);
  if (!Number.isSafeInteger(preset.version) || preset.version < 1)
    throw Error('Invalid preset version');
  const hueCells = cells(preset);
  const used = new Set();
  for (const plan of list(preset.slots, 255)) {
    fields(
      plan,
      [
        'id',
        'role',
        'position',
        'lightnessScale',
        'lightnessOffset',
        'chromaScale',
        'chromaOffset',
      ],
      ['alpha'],
    );
    if (plan.id === key.id || !slots.has(plan.id) || used.has(plan.id))
      throw Error('Missing, duplicate or key-color preset slot');
    used.add(plan.id);
    if (plan.role !== slots.get(plan.id).role)
      throw Error('Preset must preserve the stable slot role');
    if (!Number.isInteger(plan.position) || !Object.hasOwn(hueCells, plan.position))
      throw Error('Invalid preset harmony position');
    finite(plan.lightnessScale, plan.lightnessOffset, plan.chromaScale, plan.chromaOffset);
    if (plan.alpha !== undefined && !unit(plan.alpha)) throw Error('Invalid preset alpha');
  }
  return slots;
}
function resolveSlot(slot, plan, key, preset) {
  const h = norm(key.h + (cells(preset)[plan.position] * 360) / preset.hueSteps),
    baseL = key.L * plan.lightnessScale + plan.lightnessOffset,
    baseRatioCmax = key.rTarget * plan.chromaScale + plan.chromaOffset,
    requestedL = baseL + slot.deltaL,
    requestedRatio = baseRatioCmax + slot.deltaRatioCmax;
  finite(baseL, baseRatioCmax, requestedL, requestedRatio);
  const L = clamp(requestedL),
    rTarget = clamp(requestedRatio),
    cmax = gamut(L, h).cmax,
    sample = gamut(L, h, rTarget * cmax, plan.alpha ?? 1);
  return {
    id: slot.id,
    role: slot.role,
    active: true,
    value: sample.value,
    linear: [...linearFromLch(L, sample.C, h), sample.value.alpha],
    L,
    C: sample.C,
    h,
    cmax,
    baseL,
    baseRatioCmax,
    ...ratioFields(sample.C, cmax, rTarget),
    diagnostics: [
      ...(requestedL !== L ? ['lightness-clamped'] : []),
      ...(requestedRatio !== rTarget ? ['ratio-clamped'] : []),
      ...(cmax === 0 ? ['zero-cmax'] : []),
      ...(sample.gamutLimited ? ['gamut-limited'] : []),
    ],
  };
}
function solve(intent, previous = [], onlyId = null) {
  validateIntent(intent);
  const old = new Map(previous.map((row) => [row.id, row])),
    key = onlyId === null ? keySample(intent.keyColor) : old.get(intent.keyColor.id),
    plans = new Map(intent.preset.slots.map((plan) => [plan.id, plan]));
  return [
    key,
    ...intent.slots.map((slot) => {
      const plan = plans.get(slot.id);
      if (!plan) {
        if (!old.has(slot.id)) throw Error('New slot requires an explicit preset mapping');
        return { ...old.get(slot.id), active: false };
      }
      if (onlyId !== null && slot.id !== onlyId) return old.get(slot.id);
      return resolveSlot(slot, plan, key, intent.preset);
    }),
  ];
}
const diagnostics = (intent) =>
  intent.settings.areaAware ? ['coverage-unavailable-unweighted'] : [];

export function createKeyColorRecipe(input) {
  fields(input, ['paletteId', 'keyColor', 'preset', 'slots'], ['settings']);
  id(input.paletteId);
  fields(input.keyColor, ['id', 'role', 'value'], ['hueHint']);
  const intent = structuredClone({
    keyColor: {
      ...input.keyColor,
      hueHint: input.keyColor.hueHint === undefined ? 0 : input.keyColor.hueHint,
    },
    preset: input.preset,
    slots: input.slots,
    settings: input.settings === undefined ? { areaAware: true } : input.settings,
  });
  const resolved = solve(intent);
  intent.keyColor.hueHint = resolved[0].h;
  return {
    version: KEY_COLOR_RECIPE_VERSION,
    solverVersion: KEY_COLOR_SOLVER_VERSION,
    colorSemanticsVersion: KEY_COLOR_SEMANTICS_VERSION,
    paletteId: input.paletteId,
    revision: 0,
    intent,
    resolved,
    diagnostics: diagnostics(intent),
  };
}

// Validation/copy only: reload never runs the solver or quantizes stored floats.
// Valid prototype v1 snapshots gain ratio metadata / format v2 only; authored
// intent, last committed colors, diagnostics and revision remain unchanged.
export function restoreKeyColorRecipe(saved) {
  fields(saved, [
    'version',
    'solverVersion',
    'colorSemanticsVersion',
    'paletteId',
    'revision',
    'intent',
    'resolved',
    'diagnostics',
  ]);
  if (
    ![1, KEY_COLOR_RECIPE_VERSION].includes(saved.version) ||
    saved.solverVersion !== KEY_COLOR_SOLVER_VERSION ||
    saved.colorSemanticsVersion !== KEY_COLOR_SEMANTICS_VERSION ||
    !Number.isSafeInteger(saved.revision) ||
    saved.revision < 0
  )
    throw Error('Unsupported key-color snapshot version or revision');
  id(saved.paletteId);
  const slots = validateIntent(saved.intent),
    used = new Set(),
    plans = new Set(saved.intent.preset.slots.map((plan) => plan.id));
  for (const row of list(saved.resolved)) {
    const isKey = row?.id === saved.intent.keyColor.id;
    fields(
      row,
      [
        'id',
        'role',
        'active',
        'value',
        'linear',
        'L',
        'C',
        'h',
        'cmax',
        'effectiveRatioCmax',
        ...(saved.version >= 2 ? ['rTarget', 'rRealized'] : []),
        'diagnostics',
      ],
      isKey ? [] : ['baseL', 'baseRatioCmax'],
    );
    if (
      !slots.has(row.id) ||
      used.has(row.id) ||
      row.role !== slots.get(row.id).role ||
      row.active !== (isKey || plans.has(row.id))
    )
      throw Error('Invalid resolved stable slot identity');
    used.add(row.id);
    literal(row.value);
    if (
      !unit(row.L) ||
      !unit(row.effectiveRatioCmax) ||
      !Number.isFinite(row.C) ||
      row.C < 0 ||
      row.C > MAX_CHROMA ||
      !Number.isFinite(row.cmax) ||
      row.cmax < 0 ||
      row.cmax > MAX_CHROMA ||
      (!isKey && row.C > row.cmax + 1e-9) ||
      !Number.isFinite(row.h) ||
      row.h < 0 ||
      row.h >= 360 ||
      !Array.isArray(row.linear) ||
      row.linear.length !== 4 ||
      !Array.from(row.linear).every(unit) ||
      row.linear[3] !== row.value.alpha
    )
      throw Error('Invalid resolved color');
    if (!isKey) finite(row.baseL, row.baseRatioCmax);
    for (const code of list(row.diagnostics, 8))
      if (typeof code !== 'string' || !code) throw Error('Invalid color diagnostic');
    const rRealized = row.cmax === 0 ? 0 : row.C / row.cmax,
      rawRatio = isKey ? rRealized : row.baseRatioCmax + slots.get(row.id).deltaRatioCmax,
      rTarget = clamp(rawRatio);
    finite(rRealized, rawRatio);
    if (
      row.effectiveRatioCmax !== rTarget ||
      (saved.version >= 2 && (row.rTarget !== rTarget || row.rRealized !== rRealized)) ||
      (row.cmax === 0 && (row.C !== 0 || !row.diagnostics.includes('zero-cmax'))) ||
      (!isKey && row.C > row.cmax * rTarget + 1e-12)
    )
      throw Error('Invalid target/realized chroma ratio metadata');
    if (
      isKey &&
      (row.value.hex !== saved.intent.keyColor.value.hex ||
        row.value.alpha !== saved.intent.keyColor.value.alpha ||
        hexLinear(row.value.hex).some((channel, i) => row.linear[i] !== channel))
    )
      throw Error('Key-color snapshot must preserve exact input and decoded linear channels');
  }
  if (used.size !== slots.size) throw Error('Missing last committed resolved color');
  for (const code of list(saved.diagnostics, 8))
    if (typeof code !== 'string' || !code) throw Error('Invalid recipe diagnostic');
  const restored = structuredClone(saved);
  if (saved.version === 1) {
    restored.version = KEY_COLOR_RECIPE_VERSION;
    for (const row of restored.resolved)
      Object.assign(row, ratioFields(row.C, row.cmax, row.effectiveRatioCmax));
  }
  return restored;
}

// One detached candidate is one future Session/Undo transaction. Caller replaces
// the committed snapshot only on success; errors leave its intent/colors/revision intact.
export function editKeyColorRecipe(committed, edit) {
  const candidate = restoreKeyColorRecipe(committed);
  fields(
    edit,
    ['type', 'expectedRevision'],
    ['value', 'slotId', 'deltaL', 'deltaRatioCmax', 'preset'],
  );
  if (edit.expectedRevision !== candidate.revision) throw Error('Stale palette revision');
  const { intent } = candidate;
  let onlyId = null;
  if (edit.type === 'key-color') {
    fields(edit, ['type', 'expectedRevision', 'value']);
    literal(edit.value);
    intent.keyColor.value = structuredClone(edit.value);
  } else if (edit.type === 'slot-delta') {
    fields(edit, ['type', 'expectedRevision', 'slotId'], ['deltaL', 'deltaRatioCmax']);
    const slot = intent.slots.find((entry) => entry.id === edit.slotId);
    if (!slot || !intent.preset.slots.some((plan) => plan.id === slot.id))
      throw Error('Expected an active derived stable slot ID');
    if (!Object.hasOwn(edit, 'deltaL') && !Object.hasOwn(edit, 'deltaRatioCmax'))
      throw Error('Missing slot delta');
    for (const key of ['deltaL', 'deltaRatioCmax'])
      if (Object.hasOwn(edit, key)) slot[key] = edit[key];
    onlyId = slot.id;
  } else if (edit.type === 'preset') {
    fields(edit, ['type', 'expectedRevision', 'preset']);
    intent.preset = structuredClone(edit.preset);
  } else if (edit.type === 'recompute') {
    fields(edit, ['type', 'expectedRevision']);
  } else throw Error('Unsupported key-color edit');
  candidate.resolved = solve(intent, candidate.resolved, onlyId);
  intent.keyColor.hueHint = candidate.resolved[0].h;
  if (!Number.isSafeInteger(candidate.revision + 1)) throw Error('Palette revision exhausted');
  candidate.revision++;
  candidate.diagnostics = diagnostics(intent);
  return candidate;
}

// Boundary adapter only, not a generic Field implementation. No HEX round trip.
export function keyColorSlotToLinear(committed, slotId) {
  const snapshot = restoreKeyColorRecipe(committed),
    row = snapshot.resolved.find((entry) => entry.id === slotId);
  if (!row) throw Error('Missing stable palette slot');
  return {
    paletteId: snapshot.paletteId,
    paletteRevision: snapshot.revision,
    slotId,
    colorSemanticsVersion: snapshot.colorSemanticsVersion,
    type: 'ColorF32V1',
    space: 'linear-srgb',
    alphaMode: 'straight',
    value: row.linear.map(Math.fround),
  };
}
