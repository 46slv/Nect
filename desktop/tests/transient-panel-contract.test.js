import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

test('toolbar popovers share exclusive open and dismissal paths', async () => {
  const source = await readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  assert.match(source, /function closeTransientPanels\(except = ''\)/);
  assert.match(source, /if \(!gridOpen\) closeTransientPanels\('grid'\)/);
  assert.match(source, /if \(!layoutOpen\) closeTransientPanels\('layout'\)/);
  assert.match(source, /if \(!guidesOpen\) closeTransientPanels\('guides'\)/);
  assert.match(source, /if \(opening\) closeTransientPanels\('align'\)/);
  assert.match(source, /canvas-toolbar,\.guides-panel,\.alignment-panel/);
  assert.match(source, /event\.key !== 'Escape'/);
});
