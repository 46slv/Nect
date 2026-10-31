// Temporary, read-only formatter receipt. Remove before final acceptance.
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
    "path": "desktop/tests/asset-drag.e2e.js",
    "gitBlob": "3e79a818da567b78f1b0b1c828b262c57b7bbd11",
    "sha256": "dcfd2a5c439090fe6556540fce997e41d8fc16415cfc6caa78c268b00e3bc43f"
  },
  {
    "path": "desktop/tests/command-menu-actions.test.js",
    "gitBlob": "69e1f58235c17178a94746f9edf303ddb4ae822f",
    "sha256": "e1af6e677ea377a8b9972d0c511f53bdc325473d35de1435823c4d47e21c7428"
  }
];
const root = fileURLToPath(new URL('../../', import.meta.url));
const hash = (value) => createHash('sha256').update(value).digest('hex');
const blob = (value) => createHash('sha1').update(`blob ${Buffer.byteLength(value)}\0`).update(value).digest('hex');
test('capture installed formatter for frozen UI candidate', { skip: !process.env.CI }, async () => {
  const git = (...args) => execFileSync('git', args, { cwd: root, encoding: 'utf8' }).trim();
  const receipt = { runId: process.env.GITHUB_RUN_ID, checkout: git('rev-parse', 'HEAD'), tree: git('rev-parse', 'HEAD^{tree}'), prettier: prettier.version, files: [], errors: [] };
  if (prettier.version !== '3.6.2') receipt.errors.push('Unexpected installed Prettier version');
  for (const expected of files) try {
    const path = resolve(root, expected.path); const original = await readFile(path, 'utf8');
    assert.equal(blob(original), expected.gitBlob); assert.equal(hash(original), expected.sha256);
    const formatted = await prettier.format(original, { ...(await prettier.resolveConfig(path)), filepath: path });
    receipt.files.push({ ...expected, formattedSha256: hash(formatted), formatted });
    assert.equal(await readFile(path, 'utf8'), original);
  } catch (error) { receipt.errors.push(`${expected.path}: ${error.message}`); }
  const directory = resolve(root, 'desktop/test-results'); await mkdir(directory, { recursive: true });
  await writeFile(resolve(directory, 'workflow-ui-formatter.json'), JSON.stringify(receipt)); assert.deepEqual(receipt.errors, []);
});
