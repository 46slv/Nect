import test from 'node:test';
import assert from 'node:assert/strict';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';

test('main data-URL loader resolves remaining static and dynamic relative imports', () => {
  const source = `import text from './text-wrap.js'; const load = () => import('./fonts.js');`,
    resolved = resolveMainRelativeImports(source);

  assert.match(resolved, /from 'file:\/\/\/.*\/src\/text-wrap\.js'/);
  assert.match(resolved, /import\('file:\/\/\/.*\/src\/fonts\.js'\)/);
  assert.doesNotMatch(resolved, /from '\.\//);
  assert.doesNotMatch(resolved, /import\('\.\//);
});
