// Temporary read-only CI formatter receipt; remove before final verification.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import * as prettier from 'prettier';
const files = [
  {
    "path": "desktop/src/command-menus.js",
    "gitBlob": "04151df05359851ead36360f19c9f60722838548",
    "sha256": "57c4077ecda05d58258ee55d0360070134b7d39aa2c67073430cf02ddac69eb1"
  },
  {
    "path": "desktop/src/main.js",
    "gitBlob": "82712791fe24306a69508ad1910de3a295b4ea5f",
    "sha256": "c8be35a979c93334b498ec314a9bff185dd19b0abf427cf1a2729cc0ea46ddc1"
  },
  {
    "path": "desktop/src/style.css",
    "gitBlob": "569a4b8a9aa7653ab91e9b934a82356fe4f36e5c",
    "sha256": "5f002960e7d693e080b96fe9660fb9e0d98a9b30e97c2352c66ae4786733d83e"
  },
  {
    "path": "desktop/src/workspace-panels.js",
    "gitBlob": "ad6e96463117f54d55ea5d0f1f84668df4d8daf1",
    "sha256": "98eb0683da7bdd97b84d365d79bdc642a0271ce97134bf89f3f8fa16377d91b9"
  },
  {
    "path": "desktop/src/ui-icons.js",
    "gitBlob": "cacb4630909eab23057e1f8134102e44d4174371",
    "sha256": "554394bdb1716c2c772dbc6290f14a1f8d20cd9ee0ca4617745887dc0432f775"
  },
  {
    "path": "desktop/tests/command-menus.test.js",
    "gitBlob": "11a8afbe0e5cdfbaa87276d1a799554f0248b851",
    "sha256": "891435971dd51bb5e8766e99348351f553e9a501e322e4cea41f87fb4c427718"
  },
  {
    "path": "desktop/tests/workspace-panels.e2e.js",
    "gitBlob": "727df73c0e0fa81b24043f7d6d0cd6566d095049",
    "sha256": "26fe8ca7bb366b4392a3f3a46e630eaab0c99aff08fdcba5ad64141a3c4cdb4e"
  },
  {
    "path": "desktop/tests/workspace-panels.test.js",
    "gitBlob": "b46197fa626e28ee01770631f85cf90c41e13ae6",
    "sha256": "2222ac600d24af2bd95850d8d4ebcd7551dd1868850054734f4c2813cbba6997"
  },
  {
    "path": "desktop/tests/ui-icons.test.js",
    "gitBlob": "b296af8984b811bc6c8509c29922e533a7ff41d3",
    "sha256": "5fc715915596a15408ae5d9c00c0a7055c3b518acd9a36647a2b1f059f49f0f4"
  }
];
const root = fileURLToPath(new URL('../../', import.meta.url));
const hash = (value) => createHash('sha256').update(value).digest('hex');
const blob = (value) => createHash('sha1').update(`blob ${Buffer.byteLength(value)}\0`).update(value).digest('hex');
test('capture formatter output for frozen UI candidate', { skip: !process.env.CI }, async () => {
  const git = (...args) => execFileSync('git', args, { cwd: root, encoding: 'utf8' }).trim();
  const receipt = { runId: process.env.GITHUB_RUN_ID, checkout: git('rev-parse', 'HEAD'), tree: git('rev-parse', 'HEAD^{tree}'), prettier: prettier.version, files: [], errors: [] };
  if (prettier.version !== '3.6.2') receipt.errors.push('Unexpected installed Prettier version');
  for (const expected of files) {
    try {
      const path = resolve(root, expected.path);
      const original = await readFile(path, 'utf8');
      assert.equal(blob(original), expected.gitBlob, `Unexpected preimage: ${expected.path}`);
      assert.equal(hash(original), expected.sha256, `Unexpected bytes: ${expected.path}`);
      const formatted = await prettier.format(original, { ...(await prettier.resolveConfig(path)), filepath: path });
      receipt.files.push({ ...expected, formattedSha256: hash(formatted), formatted });
      assert.equal(await readFile(path, 'utf8'), original);
    } catch (error) { receipt.errors.push(`${expected.path}: ${error.message}`); }
  }
  const directory = resolve(root, 'desktop/test-results');
  await mkdir(directory, { recursive: true });
  await writeFile(resolve(directory, 'workflow-ui-formatter.json'), JSON.stringify(receipt));
  assert.deepEqual(receipt.errors, []);
});
