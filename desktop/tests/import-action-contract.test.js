import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

test('Import adds all complete source Compositions without conflating Open or Add SVG', async () => {
  const source = await readFile(new URL('../src/main.js', import.meta.url), 'utf8'),
    importStart = source.indexOf('async function openCompositionImportDialog() {'),
    importEnd = source.indexOf('function adoptSVGPart(', importStart),
    importFlow = source.slice(importStart, importEnd);
  assert.match(source, /data-action="import-svg"/);
  assert.match(source, /data-action="import-image"/);
  assert.match(source, /case 'import-svg':/);
  assert.match(source, /await importDesktopSVG\(\)/);
  assert.match(source, /case 'import-composition':/);
  assert.match(source, /await openCompositionImportDialog\(\)/);
  assert.match(importFlow, /source = deserializeProject\(opened\.contents\)/);
  assert.match(importFlow, /await documents\.releaseUnused\(opened\.binding\)/);
  assert.match(importFlow, /documents\.hasDraft\(\)/);
  assert.match(importFlow, /for \(const composition of source\.compositions\)/);
  assert.match(importFlow, /type: 'import-project-compositions'/);
  assert.doesNotMatch(importFlow, /prepareOpenDocument\(/);
  assert.match(source, /case 'open':[\s\S]*?prepareOpenDocument\(next/);
  assert.match(source, /data-action="add-svg-part"/);
  assert.match(source, /type: 'import-svg'/);
  assert.match(source, /dialog\.showModal\(\)/);
  assert.match(source, /value !== 'import'/, 'cancel and Escape must not dispatch import');
  assert.match(source, /dialog\.remove\(\)/, 'repeated opens must not retain stale dialogs');
});
