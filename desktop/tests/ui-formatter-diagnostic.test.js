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
    "path": "desktop/src/build-info.js",
    "gitBlob": "b8fe1131447d9910e173ac82c45d9a7131c6b456",
    "sha256": "19f602a3f36868dcaff14601bfd81a8e6488b5517d9b8d2dcbc553230980b49c"
  },
  {
    "path": "desktop/src/command-menus.js",
    "gitBlob": "d84ff97596ddb06da0076931930a79fcb5b31362",
    "sha256": "88442f20e1c93b7ee736b24163331b93e97e8b650a391c08327b991d2e982925"
  },
  {
    "path": "desktop/src/main.js",
    "gitBlob": "60b05eaf5e03450ae77fe3da600af9dc01acac07",
    "sha256": "405f65f67d295a72efedd096644d732e446316908ce60bbf7e0b6dabdd677a08"
  },
  {
    "path": "desktop/src/style.css",
    "gitBlob": "cd83dc5db32e66fd5746eb53714bc76bceba4c88",
    "sha256": "2c0eeafec0069421425a2ecfefef2849a3fc56ccf92790dd8456e24dfe702e38"
  },
  {
    "path": "desktop/src/text-tool.js",
    "gitBlob": "d1771ff78a491a1c1a52676e8030000332b0f7d2",
    "sha256": "82900d2959efb5c4c9da8b0f54e189a19a6d3f2424b5315501fb768aae0e1400"
  },
  {
    "path": "desktop/src/workspace-panels.js",
    "gitBlob": "4ba8a31d03a4f868e8d13201d02a5367b627152e",
    "sha256": "04c2dd104723012ace9a03180a34667e9207a8a20cecc9c8063fe0bb1be62ce5"
  },
  {
    "path": "desktop/tests/artboard-units.e2e.js",
    "gitBlob": "48b0a25317ba0cceddacaaaa87f626d1c127ba83",
    "sha256": "0faece7ad40404724302d32b56810415201511a0a5b7745735fe9a10890b5fa0"
  },
  {
    "path": "desktop/tests/artwork-ordering.e2e.js",
    "gitBlob": "e21a8aa42dc89ac3a770bef41e821213667aa8f5",
    "sha256": "8df1a38f0b2340a4b1dd344aee470651d6475a4acc485db2124e9c1973d93b15"
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
    "path": "desktop/tests/command-menu-actions.js",
    "gitBlob": "b26efe1584cc8052e1f06b276c5857f06ea03bf3",
    "sha256": "a2be444d329703ff671b31e2500aee79510e09aa91837086ddcb446e8113a49e"
  },
  {
    "path": "desktop/tests/command-menus.test.js",
    "gitBlob": "594799eb109ba87d1396604f51f7ca5149664343",
    "sha256": "95a3e9e6adf790a1fae6523d7e3cef4aa813b913b91501668d3e048424428a75"
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
    "path": "desktop/tests/graph-pan-joystick-app-dom.test.js",
    "gitBlob": "2ed30927c029f57904654b261e0f6bf67b641c85",
    "sha256": "b24b69b18bccf37590b9afdaec8f5370208a4d0917e16e15869d4ae49ff29c86"
  },
  {
    "path": "desktop/tests/images.e2e.js",
    "gitBlob": "0623a648fe983e8e7af339e98f54562908348a39",
    "sha256": "e033c7deaf812576fd119c529617a44a59ec40a2220703e29a4a892bd8316378"
  },
  {
    "path": "desktop/tests/menu-mutation-observer.test.js",
    "gitBlob": "0cdbfd0fa51c3187a3477a5a35ae7ff9f18aa57a",
    "sha256": "417c68647336cea304021c36f8b6f81661113cdd04e24277186c9d69f20fb4e2"
  },
  {
    "path": "desktop/tests/pen-menu-keyboard-dom.test.js",
    "gitBlob": "6c366c7137593286a6c15853d43bdd87c7b717aa",
    "sha256": "b0725c9301e1a524ef1f5f7e38ecc8d5eb1162883cf91fef6b630d8cfb810747"
  },
  {
    "path": "desktop/tests/project-composition-import.e2e.js",
    "gitBlob": "f3318281bbdf8b9d198e4292d3082dbad3ec02ef",
    "sha256": "e07243f3e51d6843da46bc68fcf4a4c0dec18cd70ea86c4775db8f34c06944b3"
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
    "gitBlob": "36633f9824103cb172d21da4752bfe2cf92281ac",
    "sha256": "65b7dd1efb180d7d7b6424b9cf9d9bafa1ad90037a301873d5fba33b0b478c27"
  },
  {
    "path": "desktop/tests/workspace-panels.test.js",
    "gitBlob": "9607bc1f7e42a739c4612e4e9df03a1e62804d8b",
    "sha256": "86d275e5c236230c8136e421c2205e0a65774e6f188d9f73f3c556c6d4c6128a"
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
  const path = resolve(root, expected.path); const original = await readFile(path, 'utf8');
  assert.equal(blob(original), expected.gitBlob); assert.equal(hash(original), expected.sha256);
  const formatted = await prettier.format(original, { ...(await prettier.resolveConfig(path)), filepath: path });
  receipt.files.push({ ...expected, formattedSha256: hash(formatted), formatted });
  assert.equal(await readFile(path, 'utf8'), original);
 } catch (error) { receipt.errors.push(`${expected.path}: ${error.message}`); }
 const directory = resolve(root, 'desktop/test-results'); await mkdir(directory, { recursive: true });
 await writeFile(resolve(directory, 'workflow-ui-formatter.json'), JSON.stringify(receipt)); assert.deepEqual(receipt.errors, []);
});
