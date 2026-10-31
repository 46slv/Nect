// Temporary read-only CI diagnostic; remove before final acceptance.
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
    "path": "desktop/src/asset-library.js",
    "gitBlob": "d5506a23dc095e6f3a2989f73197e269cd717b57",
    "sha256": "458991450dd922cc70cf522e16dfa018a8ab59fb35f3be0f390c99c53090a9c2"
  },
  {
    "path": "desktop/src/command-menus.js",
    "gitBlob": "671e204c22fa71fe7fe37738403d71c4120cd57d",
    "sha256": "c5fca2bfe09cce33b00e80af46500629d8fb2a09dbe4f5aab57b600da9f3ae80"
  },
  {
    "path": "desktop/src/main.js",
    "gitBlob": "eab21ba8a7287d7496b0c62a99f5d7600be98218",
    "sha256": "7b52174405415ea2d951939c8cefa4cf83ea9d6c7912f2bcd5129c07c152a7fb"
  },
  {
    "path": "desktop/src/style.css",
    "gitBlob": "c16731063fbb4b7ff60994ab56c480145b53015d",
    "sha256": "66aad8919d1c4ee98b25263a4aa8bfd808051337aabfc7a689f023f394691e49"
  },
  {
    "path": "desktop/src/text-tool.js",
    "gitBlob": "d1771ff78a491a1c1a52676e8030000332b0f7d2",
    "sha256": "82900d2959efb5c4c9da8b0f54e189a19a6d3f2424b5315501fb768aae0e1400"
  },
  {
    "path": "desktop/src/workspace-panels.js",
    "gitBlob": "d2fde764fc9369e8d74f9e1738414cafdd5b1d86",
    "sha256": "26dacec56f9bb1ef80090b07cb70b156e4f0776f667ac5e4a77e1c81621e88bb"
  },
  {
    "path": "desktop/tests/artboard-units.e2e.js",
    "gitBlob": "48b0a25317ba0cceddacaaaa87f626d1c127ba83",
    "sha256": "0faece7ad40404724302d32b56810415201511a0a5b7745735fe9a10890b5fa0"
  },
  {
    "path": "desktop/tests/asset-drag.e2e.js",
    "gitBlob": "4d5877a5e86dcad16e9a2dfc0c3572b3fafbedc8",
    "sha256": "8675c051524326b147a2953b99d27857508a7506744385bc79304f6f34c79a39"
  },
  {
    "path": "desktop/tests/canvas-transient-panels.e2e.js",
    "gitBlob": "d387cb2097f5ecc199bc41b8daf3370bd5ac3a02",
    "sha256": "28569f3c2536201cc96c6189d8b814fa3a74e8b66c2e4871a5215abd96e5147d"
  },
  {
    "path": "desktop/tests/command-menus.test.js",
    "gitBlob": "6da6149d774b344e97a10d3decdd10391c3e66c9",
    "sha256": "c7db195dcb4373568dad823a6f089bf56716cf00f23850dad4f54cf56b53c714"
  },
  {
    "path": "desktop/tests/editor-names-bypass-dom.test.js",
    "gitBlob": "a767221525f0995ef1e298b04db9fba3e192e650",
    "sha256": "bd713215b2d7e3df0b0ff718226b21729bca067c5ad3e82e7dcfadee17224fc6"
  },
  {
    "path": "desktop/tests/editor-names-bypass.e2e.js",
    "gitBlob": "159ca09733dabec6bf9601526e075c3627675ded",
    "sha256": "7754b9a9e4250581f2700e55dc6a8b877b07a8be2eb080d5d5172ac37b76caa5"
  },
  {
    "path": "desktop/tests/images.e2e.js",
    "gitBlob": "0623a648fe983e8e7af339e98f54562908348a39",
    "sha256": "e033c7deaf812576fd119c529617a44a59ec40a2220703e29a4a892bd8316378"
  },
  {
    "path": "desktop/tests/shared-colors-panel.e2e.js",
    "gitBlob": "45076bb0b3afa454e2f98988ef0e705ec2e838a6",
    "sha256": "e8a08757ecbcffdaf997b52d52e575d4b5c62246149bb48d030fb1f95455b5aa"
  },
  {
    "path": "desktop/tests/text-tool-dom.test.js",
    "gitBlob": "a2215054b62c982a3f020c7afd37a88619727c79",
    "sha256": "b834eea361e630344775855c275ea276d3a2fbf6cc6fe6a253b75a0c519a819c"
  },
  {
    "path": "desktop/tests/ui-review-captures.e2e.js",
    "gitBlob": "73846ecd3566f0f23e9b4d7e794fa751be9a4d58",
    "sha256": "80a6444e50f9d38d2ecd8c634d8d12a8379ab7281d1f1fdbaf3a00f7489c8101"
  },
  {
    "path": "desktop/tests/workspace-panels.e2e.js",
    "gitBlob": "b28ea3bbf2a7b61f9f74862dc30a84818598b284",
    "sha256": "8fd247246049e096f5189c2a8ecefcb97b18e3a07159bd11225a785f3a32458a"
  },
  {
    "path": "desktop/tests/workspace-panels.test.js",
    "gitBlob": "aa81aa010991c3751411f48da332d710f69fec08",
    "sha256": "cea9ec6d3a367d07ece785db0a5a9a305fb61f37b920af35c9eb9c18b62c9ffc"
  },
  {
    "path": "desktop/src/ui-icons.js",
    "gitBlob": "b1ade5dbf0f660edf8583d793724b32813137843",
    "sha256": "21f774934d79318bd81ed59000067ab298ade752c216f31e28f75b93ebac40e9"
  },
  {
    "path": "desktop/tests/ui-icons.test.js",
    "gitBlob": "8bbbaa1449135d12b9cf07ea70deb5e5fbab99e0",
    "sha256": "48ec58142a7b938ef4b274d1f40b2eb7405b2366de8582fbf842f1c43c20fff8"
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
    const path = resolve(root, expected.path);
    const original = await readFile(path, 'utf8');
    assert.equal(blob(original), expected.gitBlob);
    assert.equal(hash(original), expected.sha256);
    const formatted = await prettier.format(original, { ...(await prettier.resolveConfig(path)), filepath: path });
    receipt.files.push({ ...expected, formattedSha256: hash(formatted), formatted });
    assert.equal(await readFile(path, 'utf8'), original);
  } catch (error) { receipt.errors.push(`${expected.path}: ${error.message}`); }
  const directory = resolve(root, 'desktop/test-results');
  await mkdir(directory, { recursive: true });
  await writeFile(resolve(directory, 'workflow-ui-formatter.json'), JSON.stringify(receipt));
  assert.deepEqual(receipt.errors, []);
});
