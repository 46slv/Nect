import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

const css = await readFile(new URL('../src/style.css', import.meta.url), 'utf8'),
  main = await readFile(new URL('../src/main.js', import.meta.url), 'utf8');

test('wrapped header allocates its rows before the workspace', () => {
  const app = css.match(/#app\s*\{([^}]*)\}/)?.[1],
    header = css.match(/(?:^|\n)header\s*\{([^}]*)\}/)?.[1],
    workspace = css.match(/\.workspace\s*\{([^}]*)\}/)?.[1];

  assert.ok(app);
  assert.ok(header);
  assert.ok(workspace);
  assert.match(app, /display:\s*grid/);
  assert.match(app, /grid-template-rows:\s*auto minmax\(500px,\s*1fr\) auto/);
  assert.match(header, /height:\s*auto/);
  assert.match(header, /min-height:\s*48px/);
  assert.match(header, /flex-wrap:\s*wrap/);
  assert.match(workspace, /height:\s*auto/);
  assert.match(workspace, /min-height:\s*500px/);
  assert.match(css, /header\s*\{\s*overflow:\s*visible;\s*\}/);
  assert.match(main, /<header>[\s\S]*?<\/header><div class="workspace">/);
});
