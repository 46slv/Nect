import test from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

test('prepared Composition switches release retired CommandSessions like ordinary switches', (t) => {
  const child = spawnSync(
    process.execPath,
    [
      '--expose-gc',
      fileURLToPath(new URL('./helpers/session-retention-probe.mjs', import.meta.url)),
      fileURLToPath(new URL('../..', import.meta.url)),
      '30',
    ],
    { encoding: 'utf8', timeout: 30000, maxBuffer: 1024 * 1024 },
  );
  assert.ifError(child.error);
  assert.equal(child.status, 0, child.stderr || child.stdout);
  const result = JSON.parse(child.stdout);
  t.diagnostic(JSON.stringify(result));
  for (const route of ['synchronous', 'prepared']) {
    assert.equal(result[route].created, 31);
    assert.equal(result[route].active, 'a');
    assert.equal(result[route].alive, 1, `${route} must retain only its current Session`);
  }
});
