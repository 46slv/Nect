import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

test('canvas popovers remain exclusive while alignment uses its workspace drawer', async () => {
  const source = await readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  assert.match(source, /function closeTransientPanels\(except = ''\)/);
  assert.match(source, /if \(!gridOpen\) closeTransientPanels\('grid'\)/);
  assert.match(source, /if \(!layoutOpen\) closeTransientPanels\('layout'\)/);
  assert.match(source, /if \(!guidesOpen\) closeTransientPanels\('guides'\)/);
  assert.match(source, /workspacePanels\.select\('align'\)/);
  assert.match(source, /canvas-toolbar,\.guides-panel,\.alignment-panel/);
  assert.match(source, /event\.key !== 'Escape'/);
});
