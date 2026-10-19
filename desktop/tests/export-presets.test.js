import assert from 'node:assert/strict';
import test from 'node:test';
import {
  EXPORT_PRESETS_STORAGE_KEY,
  PNG_EXPORT_SCALES,
  applyExportPreset,
  loadExportPresets,
  saveExportPreset,
} from '../src/export-presets.js';

function memoryStorage() {
  const values = new Map();
  return {
    getItem(key) {
      return values.get(key) ?? null;
    },
    setItem(key, value) {
      values.set(key, String(value));
    },
  };
}

test('empty preset storage leaves the existing export defaults available', () => {
  const storage = memoryStorage();
  assert.deepEqual(loadExportPresets(storage), []);
  assert.equal(storage.getItem(EXPORT_PRESETS_STORAGE_KEY), null);
  assert.deepEqual(PNG_EXPORT_SCALES, [0.5, 1, 2, 4]);
});

test('SVG and PNG presets save and reload with format-specific settings', () => {
  const storage = memoryStorage();
  const svg = saveExportPreset(storage, {
    format: 'svg',
    name: '  Responsive mm  ',
    settings: { sizing: 'responsive', unit: 'mm', precision: '3' },
  });
  const pngScale = saveExportPreset(storage, {
    format: 'png',
    name: '2x',
    settings: { mode: 'scale', scale: 2 },
  });
  const pngPpi = saveExportPreset(storage, {
    format: 'png',
    name: 'Print 300',
    settings: { mode: 'ppi', ppi: 300 },
  });

  assert.equal(svg.preset.name, 'Responsive mm');
  const reloaded = loadExportPresets(storage);
  assert.equal(reloaded.length, 3);
  assert.deepEqual(applyExportPreset(reloaded[0], 'svg'), {
    sizing: 'responsive',
    unit: 'mm',
    precision: '3',
  });
  assert.deepEqual(applyExportPreset(pngScale.preset, 'png'), { mode: 'scale', scale: 2 });
  assert.deepEqual(applyExportPreset(pngPpi.preset, 'png'), { mode: 'ppi', ppi: 300 });
  assert.throws(() => applyExportPreset(svg.preset, 'png'), /format does not match/);
});

test('saving an existing same-format name updates only that preset', () => {
  const storage = memoryStorage();
  saveExportPreset(storage, {
    format: 'svg',
    name: 'Print',
    settings: { sizing: 'fixed', unit: 'mm', precision: 'default' },
  });
  const updated = saveExportPreset(storage, {
    format: 'svg',
    name: 'Print',
    settings: { sizing: 'fixed', unit: 'cm', precision: '4' },
  });

  assert.equal(updated.presets.length, 1);
  assert.deepEqual(applyExportPreset(loadExportPresets(storage)[0], 'svg'), {
    sizing: 'fixed',
    unit: 'cm',
    precision: '4',
  });
});

test('invalid settings and extra cross-format fields reject without changing saved data', () => {
  const storage = memoryStorage();
  const existing = saveExportPreset(storage, {
    format: 'svg',
    name: 'Good',
    settings: { sizing: 'fixed', unit: 'px', precision: 'default' },
  });
  const before = storage.getItem(EXPORT_PRESETS_STORAGE_KEY);
  const invalid = [
    {
      format: 'svg',
      name: 'Bad precision',
      settings: { sizing: 'fixed', unit: 'px', precision: '1' },
    },
    {
      format: 'svg',
      name: 'Extra path',
      settings: { sizing: 'fixed', unit: 'px', precision: '2', path: 'C:\\output.svg' },
    },
    { format: 'png', name: 'Extra mode field', settings: { mode: 'ppi', ppi: 300, scale: 2 } },
    { format: 'png', name: 'Bad scale', settings: { mode: 'scale', scale: 1.5 } },
    { format: 'png', name: 'Bad PPI', settings: { mode: 'ppi', ppi: 0 } },
    { format: 'pdf', name: 'Unsupported', settings: {} },
  ];
  for (const preset of invalid) {
    assert.throws(() => applyExportPreset(preset, preset.format), /Invalid|format must/);
    assert.throws(() => saveExportPreset(storage, preset), /Invalid|format must/);
    assert.equal(storage.getItem(EXPORT_PRESETS_STORAGE_KEY), before);
  }
  assert.throws(() => applyExportPreset(existing.preset, 'png'), /format does not match/);
});

test('corrupt stored presets refuse loading and cannot be overwritten by a new save', () => {
  const storage = memoryStorage();
  const corrupt = JSON.stringify({
    version: 1,
    presets: [
      { format: 'svg', name: 'Bad', settings: { sizing: 'fixed', unit: 'px', precision: 1 } },
    ],
  });
  storage.setItem(EXPORT_PRESETS_STORAGE_KEY, corrupt);
  assert.throws(() => loadExportPresets(storage), /Invalid SVG/);
  assert.throws(
    () =>
      saveExportPreset(storage, {
        format: 'png',
        name: 'New',
        settings: { mode: 'scale', scale: 2 },
      }),
    /Invalid SVG/,
  );
  assert.equal(storage.getItem(EXPORT_PRESETS_STORAGE_KEY), corrupt);
});
