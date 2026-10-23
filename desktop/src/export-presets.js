import { ARTBOARD_UNITS } from './artboard-units.js';
import { SVG_EXPORT_PRECISIONS } from './svg-number-format.js';

export const EXPORT_PRESETS_STORAGE_KEY = 'nect.export-presets.v1';
export const PNG_EXPORT_SCALES = Object.freeze([0.5, 1, 2, 4]);

const STORAGE_VERSION = 1;
const MAX_EXPORT_PRESETS = 100;
const MAX_PRESET_NAME_LENGTH = 100;

function isRecord(value) {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function hasExactKeys(value, keys) {
  return (
    isRecord(value) &&
    Object.keys(value).length === keys.length &&
    keys.every((key) => Object.hasOwn(value, key))
  );
}

function normalizeName(value) {
  if (typeof value !== 'string') throw Error('Preset name must be text');
  const name = value.trim();
  if (!name || name.length > MAX_PRESET_NAME_LENGTH || /[\u0000-\u001f\u007f]/u.test(name))
    throw Error('Preset name must contain 1 to 100 printable characters');
  return name;
}

function normalizeSettings(format, settings) {
  if (format === 'svg') {
    if (
      !hasExactKeys(settings, ['sizing', 'unit', 'precision']) ||
      !['fixed', 'responsive'].includes(settings.sizing) ||
      !ARTBOARD_UNITS.includes(settings.unit) ||
      !(
        settings.precision === 'default' ||
        (typeof settings.precision === 'string' &&
          SVG_EXPORT_PRECISIONS.some((value) => String(value) === settings.precision))
      )
    )
      throw Error('Invalid SVG export preset settings');
    return {
      sizing: settings.sizing,
      unit: settings.unit,
      precision: settings.precision,
    };
  }
  if (format === 'png') {
    if (!isRecord(settings) || !['scale', 'ppi'].includes(settings.mode))
      throw Error('Invalid PNG export preset settings');
    if (
      settings.mode === 'scale' &&
      hasExactKeys(settings, ['mode', 'scale']) &&
      PNG_EXPORT_SCALES.includes(settings.scale)
    )
      return { mode: 'scale', scale: settings.scale };
    if (
      settings.mode === 'ppi' &&
      hasExactKeys(settings, ['mode', 'ppi']) &&
      Number.isFinite(settings.ppi) &&
      settings.ppi > 0
    )
      return { mode: 'ppi', ppi: settings.ppi };
    throw Error('Invalid PNG export preset settings');
  }
  throw Error('Export preset format must be SVG or PNG');
}

export function validateExportPreset(value) {
  if (!hasExactKeys(value, ['format', 'name', 'settings'])) throw Error('Invalid export preset');
  if (!['svg', 'png'].includes(value.format))
    throw Error('Export preset format must be SVG or PNG');
  return {
    format: value.format,
    name: normalizeName(value.name),
    settings: normalizeSettings(value.format, value.settings),
  };
}

export function applyExportPreset(value, expectedFormat) {
  const preset = validateExportPreset(value);
  if (!['svg', 'png'].includes(expectedFormat) || preset.format !== expectedFormat)
    throw Error('Export preset format does not match the selected export');
  return { ...preset.settings };
}

export function loadExportPresets(storage) {
  if (!storage || typeof storage.getItem !== 'function') return [];
  const text = storage.getItem(EXPORT_PRESETS_STORAGE_KEY);
  if (text === null || text === '') return [];
  let stored;
  try {
    stored = JSON.parse(text);
  } catch {
    throw Error('Saved export presets contain invalid JSON');
  }
  if (
    !hasExactKeys(stored, ['version', 'presets']) ||
    stored.version !== STORAGE_VERSION ||
    !Array.isArray(stored.presets) ||
    stored.presets.length > MAX_EXPORT_PRESETS
  )
    throw Error('Saved export presets have an unsupported format');
  const presets = stored.presets.map(validateExportPreset);
  const names = new Set();
  for (const preset of presets) {
    const key = preset.format + '\u0000' + preset.name;
    if (names.has(key)) throw Error('Saved export preset names must be unique within each format');
    names.add(key);
  }
  return presets;
}

export function saveExportPreset(storage, value) {
  if (!storage || typeof storage.getItem !== 'function' || typeof storage.setItem !== 'function')
    throw Error('Export preset storage is unavailable');
  const preset = validateExportPreset(value);
  const presets = loadExportPresets(storage);
  const existingIndex = presets.findIndex(
    (item) => item.format === preset.format && item.name === preset.name,
  );
  if (existingIndex < 0 && presets.length >= MAX_EXPORT_PRESETS)
    throw Error('Export preset limit reached');
  const next = [...presets];
  if (existingIndex < 0) next.push(preset);
  else next[existingIndex] = preset;
  storage.setItem(
    EXPORT_PRESETS_STORAGE_KEY,
    JSON.stringify({ version: STORAGE_VERSION, presets: next }),
  );
  return { preset, presets: next };
}
