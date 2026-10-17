import test from 'node:test';
import assert from 'node:assert/strict';
import { normalizeSystemFontEntry, listSystemFonts } from '../src/storage.js';

test('legacy catalog stays flat and browser catalog stays empty', async () => {
  const entry = ['system-7', 'Legacy bold filename', 'Variable fonts unsupported'];
  assert.deepEqual(normalizeSystemFontEntry(entry), entry);
  assert.deepEqual(normalizeSystemFontEntry([...entry, null]), entry);
  assert.deepEqual(await listSystemFonts(), []);
});

test('optional native descriptor preserves opaque identity, name and rejection reason', () => {
  const source = {
    family: ' Family ',
    subfamily: 'Bold',
    fullName: 'Family Bold',
    postScriptName: 'Family-Bold',
    weightClass: 700,
    widthClass: 5,
    fsSelection: 545,
    italic: true,
    bold: true,
    regular: false,
    oblique: true,
    path: '/must/not/propagate',
    id: 'another-id',
  };
  const entry = normalizeSystemFontEntry(['system-29', 'Original label', 'Unsupported', source]);
  assert.deepEqual(entry.slice(0, 3), ['system-29', 'Original label', 'Unsupported']);
  assert.deepEqual(entry[3], {
    family: 'Family',
    subfamily: 'Bold',
    fullName: 'Family Bold',
    postScriptName: 'Family-Bold',
    weightClass: 700,
    widthClass: 5,
    fsSelection: 545,
    italic: true,
    bold: true,
    regular: false,
    oblique: true,
  });
  assert.ok(Object.isFrozen(entry[3]));
  source.family = 'Changed';
  assert.equal(entry[3].family, 'Family');
});

test('unknown, invalid and missing fields remain unknown without invented regular face', () => {
  const entry = normalizeSystemFontEntry([
    'system-1',
    'Bold Italic.ttf',
    null,
    {
      family: '\u0000bad',
      subfamily: ' '.repeat(8),
      fullName: 'x'.repeat(257),
      postScriptName: 42,
      weightClass: '700',
      widthClass: 10,
      fsSelection: -1,
      italic: 1,
      bold: 'true',
      regular: undefined,
    },
  ]);
  assert.ok(Object.values(entry[3]).every((value) => value === null));
  for (const weightClass of [0, 1001, NaN, Infinity, 1.5])
    assert.equal(
      normalizeSystemFontEntry(['id', 'Name', null, { weightClass }])[3].weightClass,
      null,
    );
  assert.equal(
    normalizeSystemFontEntry(['id', 'Name', null, { fsSelection: 0 }])[3].fsSelection,
    0,
  );
});

test('same display family never collapses catalog face IDs or fabricates styles', () => {
  const faces = ['system-1', 'system-2'].map((id) =>
    normalizeSystemFontEntry([id, 'Same', null, { family: 'Same' }]),
  );
  assert.notEqual(faces[0][0], faces[1][0]);
  assert.equal(faces[0][3].subfamily, null);
  assert.equal(faces[0][3].weightClass, null);
});

test('desktop list adapter accepts mixed old and new IPC rows without reading font bytes', async () => {
  const { readFile } = await import('node:fs/promises');
  const rows = [
    ['system-5', 'Old face', null],
    ['system-8', 'New face', null, { family: 'Family', weightClass: 700 }],
  ];
  const calls = [];
  globalThis.catalogMetadataInvoke = async (command) => {
    calls.push(command);
    return rows;
  };
  let source = await readFile(new URL('../src/storage.js', import.meta.url), 'utf8');
  source = source
    .replace(
      "import { invoke, isTauri, Channel } from '@tauri-apps/api/core';",
      'const invoke = (...args) => globalThis.catalogMetadataInvoke(...args), isTauri = () => true; class Channel {}',
    )
    .replace("export { DocumentLifecycle } from './document-lifecycle.js';", '');
  try {
    const adapter = await import(
      'data:text/javascript;base64,' + Buffer.from(source).toString('base64')
    );
    const result = await adapter.listSystemFonts();
    assert.deepEqual(calls, ['list_system_fonts']);
    assert.deepEqual(result[0], rows[0]);
    assert.equal(result[1][3].family, 'Family');
    assert.equal(result[1][3].weightClass, 700);
    assert.equal(result[1][3].subfamily, null);
    assert.equal(rows[1][3].subfamily, undefined, 'does not mutate IPC responses');
  } finally {
    delete globalThis.catalogMetadataInvoke;
  }
});
