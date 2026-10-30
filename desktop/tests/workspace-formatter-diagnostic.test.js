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
    "path": "desktop/src/asset-library.js",
    "gitBlob": "563eceaf71a3c0ee5a1313538c0aa5b6e41b8cda",
    "sha256": "e7f82d8b423de2d819a3e1894b63c75c4ff907ec1ec48a37c39fea6dbf48aa1d"
  },
  {
    "path": "desktop/src/main.js",
    "gitBlob": "cf99de3ae745de45a5930dfbadc42203516187b8",
    "sha256": "9ecce65db33f8d3108a427a802b134d2abe6c88c98a131f470212e9feabe3ae0"
  },
  {
    "path": "desktop/src/shared-colors-panel.js",
    "gitBlob": "c72f3d9df20b43903719d8d043bc1d4a62e1bb57",
    "sha256": "df2a7576cb6d5e399f900b6863c4f189fa9e5576daa420117a862d07e306a90a"
  },
  {
    "path": "desktop/src/style.css",
    "gitBlob": "15b28d7a4bc676c8a0b0c78ed6a64ba83533d220",
    "sha256": "e146ae1a6cf7b4a86a674528ffabe1fd0be65c20fef89d4987edd8bde7211712"
  },
  {
    "path": "desktop/src/workspace-panels.js",
    "gitBlob": "f4852f7a51e49ad6598d1a644e92a9760349243b",
    "sha256": "1dee08e4a2c063a7909aa3e016699c86cbc47e71af9907df92f183bd34210df6"
  },
  {
    "path": "desktop/tests/asset-drag.e2e.js",
    "gitBlob": "c12a0dcd23e769b9d539caa586c5e2a2df38c01a",
    "sha256": "e8dff488532a2809b387d1cf3129fdb2be91d1b84a33e514a368b7098a67fade"
  },
  {
    "path": "desktop/tests/private-font-flow-dom.test.js",
    "gitBlob": "5cb5cd5210c6b4cbf632373b5b2ed01208a364ed",
    "sha256": "9335c63d1e2304a81c03fd3ea85acfb91c6ed49cc2d9367598ee7dce88edca79"
  },
  {
    "path": "desktop/tests/private-font-flow.e2e.js",
    "gitBlob": "6c8edcbc4ae12e67b450c7327af0d6cffe370c6d",
    "sha256": "49ca58c82c8a24f9769a4cab22705da2dc98e7c00ec8dbbbfaf273c2965ac2f1"
  },
  {
    "path": "desktop/tests/transient-panel-contract.test.js",
    "gitBlob": "7ed701a92af4d2db04dd7d2e47130cbb19866f1e",
    "sha256": "a873615615722e3f05beb5b2046b8d439db56f21538275d62062067f653d77af"
  },
  {
    "path": "desktop/tests/workspace-panels.e2e.js",
    "gitBlob": "106be895c64ed5a0c0eb1643b660cca68b17c32d",
    "sha256": "d6c0c9def0ba72392b169fe09c8de40b847b0365497af856370c8ce8332c3495"
  },
  {
    "path": "desktop/tests/workspace-panels.test.js",
    "gitBlob": "3844690fb5789da82921af9cfca3fe888aa6aa79",
    "sha256": "8823429f65c8a29cb658d0624847d996551c5a3756ee6d9c65bd10000e0dfd55"
  }
];
const root = fileURLToPath(new URL('../../', import.meta.url));
const hash = (value) => createHash('sha256').update(value).digest('hex');
const blob = (value) => createHash('sha1').update(`blob ${Buffer.byteLength(value)}\0`).update(value).digest('hex');

test('capture installed formatters for the frozen workspace candidate without source writes', { skip: !process.env.CI }, async () => {
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
  await writeFile(resolve(directory, 'workflow-workspace-formatter.json'), JSON.stringify(receipt));
  assert.deepEqual(receipt.errors, []);
});
