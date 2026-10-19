import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

test('command menu CSS does not clip details panels below the header', async () => {
  const css = await readFile(new URL('../src/style.css', import.meta.url), 'utf8');
  assert.match(css, /\.command-menu-items\s*\{[\s\S]*?position:\s*absolute[\s\S]*?z-index:\s*30/);
  assert.match(css, /header\s*\{\s*overflow:\s*visible;\s*\}/);
});
