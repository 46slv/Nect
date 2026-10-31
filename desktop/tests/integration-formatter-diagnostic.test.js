// Temporary read-only CI diagnostic. Remove before final acceptance.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { execFileSync, spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import * as prettier from 'prettier';

const files = [
  {
    "path": "desktop/docs/hsl-first-picker.md",
    "gitBlob": "80138c7fb2fb0ef75a81de0a3bd55af670620094",
    "sha256": "7a86925129aafae200a0fb0e843e9e4e820dbef44dd95eef9adbc332e2b217b0"
  },
  {
    "path": "desktop/docs/workspace-drawers.md",
    "gitBlob": "0fa55c5c436f145d50911c7c6f690122a3a1397d",
    "sha256": "0115b2f820d643c7d9c8ad7132cd393611aa62e90976919a3ffb8320fc41a2d1"
  },
  {
    "path": "desktop/scripts/browser-suite-runner.mjs",
    "gitBlob": "9259d3b9fa2ecc934ad352aac68f9e7908466c21",
    "sha256": "545d298307e043b5d61a1a3616af53273f64b410b5f61ed380d9683cf626cf25"
  },
  {
    "path": "desktop/src/hsl-color-picker.js",
    "gitBlob": "0214eef536bba5467abd26aabd4cefdb364f45f5",
    "sha256": "5e796668f67106ca4c3c77b33091bb3acba2ec7ece7693638f265868f0d01f2e"
  },
  {
    "path": "desktop/src/main.js",
    "gitBlob": "eeafd42cc0a86be7bc18b61f96595f8ef36eadf9",
    "sha256": "9fcf886b405c3ea6dcaf5e884afde0f02684d2c4c98ee18a8eebce8f57bfa8de"
  },
  {
    "path": "desktop/src/shared-colors-panel.js",
    "gitBlob": "f3329864e9a26b5653b16b2d134db3d1328a6e1c",
    "sha256": "88958f3b8973d7db48308452003ad2307a0631ec81b7b2091e4a9220cf013071"
  },
  {
    "path": "desktop/src/style.css",
    "gitBlob": "05b01b2ec622709bb33c6a980b66de121bd36201",
    "sha256": "6ce23f2f75ee62cfc943085d6f4454ad0b2c7fcb08670450535f55007fed04e3"
  },
  {
    "path": "desktop/tests/browser-suite-runner.test.js",
    "gitBlob": "a79c2901879dfd59ebd96fe009f4ee3728737d25",
    "sha256": "f4b57352b8fec575a675f8d25f8a74280d68be5944b54a837f5eb05362a6b060"
  },
  {
    "path": "desktop/tests/color-dom.test.js",
    "gitBlob": "163f5e28f4bb940c7d849f0428d58b7e3469aeb7",
    "sha256": "899c89a4617facaf6eba94df3d0035e44d3f238b276ebf2c2d6c7849f34586fe"
  },
  {
    "path": "desktop/tests/hsl-color-picker.test.js",
    "gitBlob": "112dba03b2bac67359c6baf0d62dbf2ed5d05b92",
    "sha256": "d8c33cd0b632e2ac772b7f00bb7732e9e424e9b963aaa895bdbd8630caa6ea5f"
  },
  {
    "path": "desktop/tests/hsl-workspace-integration.e2e.js",
    "gitBlob": "29eff1837d01867bf41ffd0cb1a4651ebaebc67f",
    "sha256": "6b4ae7298b0cb5ae65958c24f93c45908a7718703e73f21fcf0ca03f1553b61d"
  },
  {
    "path": "desktop/tests/shared-colors-panel.test.js",
    "gitBlob": "8af7eb065f81457a0f7f09b3bfa50d11e3f079f8",
    "sha256": "e8a64978508d50fce0d204e4301d4cc834bb8af0cf25cdf0436f35b5efefe80b"
  }
];
const root = fileURLToPath(new URL('../../', import.meta.url));
const hash = (value) => createHash('sha256').update(value).digest('hex');
const blob = (value) => createHash('sha1').update(`blob ${Buffer.byteLength(value)}\0`).update(value).digest('hex');

test('capture installed formatters for the frozen integration candidate without source writes', { skip: !process.env.CI }, async () => {
  const git = (...args) => execFileSync('git', args, { cwd: root, encoding: 'utf8' }).trim();
  const rustVersion = spawnSync('rustfmt', ['--version'], { encoding: 'utf8' });
  const receipt = {
    runId: process.env.GITHUB_RUN_ID,
    attempt: process.env.GITHUB_RUN_ATTEMPT,
    checkout: git('rev-parse', 'HEAD'),
    tree: git('rev-parse', 'HEAD^{tree}'),
    prettier: prettier.version,
    rustfmt: rustVersion.stdout?.trim(),
    rustfmtStatus: rustVersion.status,
    files: [], errors: [],
  };
  if (prettier.version !== '3.6.2') receipt.errors.push('Unexpected installed Prettier version');
  if (rustVersion.status !== 0) receipt.errors.push('Installed rustfmt unavailable');
  for (const expected of files) {
    try {
      const path = resolve(root, expected.path);
      assert.ok(path.startsWith(root));
      const original = await readFile(path, 'utf8');
      assert.equal(blob(original), expected.gitBlob, `Unexpected preimage: ${expected.path}`);
      assert.equal(hash(original), expected.sha256, `Unexpected bytes: ${expected.path}`);
      let formatted;
      if (expected.path.endsWith('.rs')) {
        // Stdin has no filesystem filename. skip_children also prevents module
        // discovery; the formatter receives exactly one allowlisted source.
        const result = spawnSync('rustfmt', ['--emit', 'stdout', '--edition', '2021', '--config', 'skip_children=true'], {
          input: original, encoding: 'utf8', maxBuffer: 16 * 1024 * 1024,
        });
        if (result.status !== 0) throw Error(`rustfmt failed: ${result.stderr}`);
        formatted = result.stdout;
      } else {
        formatted = await prettier.format(original, {
          ...(await prettier.resolveConfig(path)), filepath: path,
        });
      }
      receipt.files.push({ ...expected, formattedSha256: hash(formatted), formatted });
      assert.equal(await readFile(path, 'utf8'), original, 'Diagnostic mutated source');
    } catch (error) { receipt.errors.push(`${expected.path}: ${error.message}`); }
  }
  const directory = resolve(root, 'desktop/test-results');
  await mkdir(directory, { recursive: true });
  await writeFile(resolve(directory, 'workflow-integration-formatter.json'), JSON.stringify(receipt));
  assert.deepEqual(receipt.errors, []);
});
