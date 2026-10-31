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
    "gitBlob": "1733e1c6f71d8b2abb8083de4438715448102a21",
    "sha256": "0a20d864cd2731b1a13dc42c58ab51d3613ff93435fd058a866df45771305efd"
  },
  {
    "path": "desktop/tests/command-menu-actions.test.js",
    "gitBlob": "993f9caadf18dbf84475b35d015802ae8d4aafb8",
    "sha256": "b62b4bb41b01e0e8983d213cf27187b3cbe518fc28219c83461ccc880d4a856d"
  },
  {
    "path": "desktop/tests/harmony-wheel-panel.e2e.js",
    "gitBlob": "dc15d68c6280f8c65107859b9b00e79d03ea590c",
    "sha256": "508df7fb44e21e63d5bd3c3a0beb4aee7796d7f6960201265323e493a5c9c785"
  },
  {
    "path": "desktop/tests/shared-colors-panel.e2e.js",
    "gitBlob": "5c5075cd84b6d5581a2b45a36c15fcaf336814ac",
    "sha256": "e4ed604549066ed64419faab265b61e19fe464282b0fabc61da6ddac46250632"
  },
  {
    "path": "desktop/tests/view-navigation.e2e.js",
    "gitBlob": "93cd8426a3c0a3deabc8d8be649dc3f0b005919d",
    "sha256": "622de76dfea942c9e03135623452110bddcc16368d3e62f44408313772129239"
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
