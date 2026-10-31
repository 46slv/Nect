import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import test from 'node:test';

const source = readFileSync(new URL('./graph-pan-joystick.e2e.js', import.meta.url), 'utf8');
const moduleURL = (text) => `data:text/javascript;base64,${Buffer.from(text).toString('base64')}`;
// Stop at navigation so the real entrypoint's setup can run without a browser/server.
const playwright = moduleURL(`
  export const chromium = {
    async launch() {
      return {
        async newPage() {
          return { async goto(url) { throw new Error('navigation:' + url); } };
        },
        async close() { console.log('browser closed'); },
      };
    },
  };
`);
const entrypoint = moduleURL(
  source
    .replace("from 'playwright-core'", `from '${playwright}'`)
    .replace(
      "from './helpers/boundary-controls.js'",
      `from '${new URL('./helpers/boundary-controls.js', import.meta.url).href}'`,
    ),
);

for (const [name, baseURL, expected] of [
  ['unset BASE_URL uses the workflow Vite origin', undefined, 'http://127.0.0.1:5173'],
  ['empty BASE_URL uses the workflow Vite origin', '', 'http://127.0.0.1:5173'],
  ['explicit BASE_URL selects the owned server', 'http://127.0.0.1:5193', 'http://127.0.0.1:5193'],
]) {
  test(name, () => {
    const env = { ...process.env };
    delete env.BASE_URL;
    if (baseURL !== undefined) env.BASE_URL = baseURL;
    const result = spawnSync(
      process.execPath,
      [
        '--input-type=module',
        '-e',
        `try { await import(${JSON.stringify(entrypoint)}); }
         catch (error) { console.log(error.message); }`,
      ],
      { env, encoding: 'utf8', timeout: 10000 },
    );
    assert.ifError(result.error);
    assert.equal(result.status, 0, result.stderr);
    assert.deepEqual(result.stdout.trim().split(/\r?\n/), [
      'browser closed',
      `navigation:${expected}`,
    ]);
  });
}
