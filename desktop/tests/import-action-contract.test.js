import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

test('import actions keep the native chooser and modal lifecycle connected', async () => {
  const source = await readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  assert.match(source, /data-action="import-svg"/);
  assert.match(source, /data-action="import-image"/);
  assert.match(source, /case 'import-svg':[\s\S]*?openSVGSource/);
  assert.match(source, /case 'import-composition':[\s\S]*?openCompositionImportDialog/);
  assert.match(source, /dialog\.showModal\(\)/);
  assert.match(source, /value !== 'import'/, 'cancel and Escape must not dispatch import');
  assert.match(source, /dialog\.remove\(\)/, 'repeated opens must not retain stale dialogs');
});
