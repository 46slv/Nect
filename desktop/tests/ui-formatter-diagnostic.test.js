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
    "path": "desktop/src/asset-library.js",
    "gitBlob": "d5506a23dc095e6f3a2989f73197e269cd717b57",
    "sha256": "458991450dd922cc70cf522e16dfa018a8ab59fb35f3be0f390c99c53090a9c2"
  },
  {
    "path": "desktop/src/command-menus.js",
    "gitBlob": "04151df05359851ead36360f19c9f60722838548",
    "sha256": "57c4077ecda05d58258ee55d0360070134b7d39aa2c67073430cf02ddac69eb1"
  },
  {
    "path": "desktop/src/main.js",
    "gitBlob": "b48bee4f8d9ab9662e41ff4c2568dc0acd323f8a",
    "sha256": "6cb08f74c7679649188f2a05f22056b7f400ed503746b23fedf61513e3ee4d91"
  },
  {
    "path": "desktop/src/style.css",
    "gitBlob": "82858f3caf681e60bd375526771cdbc1d438389b",
    "sha256": "8aee4d6cc6e6ced9f216b3b1af4e360bc5674cbf98c631563d8fc0c6d759861b"
  },
  {
    "path": "desktop/src/text-tool.js",
    "gitBlob": "d1771ff78a491a1c1a52676e8030000332b0f7d2",
    "sha256": "82900d2959efb5c4c9da8b0f54e189a19a6d3f2424b5315501fb768aae0e1400"
  },
  {
    "path": "desktop/src/ui-icons.js",
    "gitBlob": "2e76f31e70f0097bd177ba14d023c90064a01c20",
    "sha256": "4873e6801022394d6686c1661db461b19f9b7268e4736dbe1128504e0865ea7f"
  },
  {
    "path": "desktop/src/workspace-panels.js",
    "gitBlob": "341ed52f83996022c134cc8becb6a7e2b8c27897",
    "sha256": "63ff8aa44b2947a8f0de74a5b8e4048c3fe038fcdfe0b2d48590b76f8d042530"
  },
  {
    "path": "desktop/tests/asset-drag.e2e.js",
    "gitBlob": "e7027e0e834fd9e5f027e686667b52ed609d09b8",
    "sha256": "a41d504eb9b74fcdfccbde7d61c380e8c486179cb3335991345ce5c790d03a06"
  },
  {
    "path": "desktop/tests/canvas-transient-panels.e2e.js",
    "gitBlob": "d387cb2097f5ecc199bc41b8daf3370bd5ac3a02",
    "sha256": "28569f3c2536201cc96c6189d8b814fa3a74e8b66c2e4871a5215abd96e5147d"
  },
  {
    "path": "desktop/tests/command-menus.test.js",
    "gitBlob": "11a8afbe0e5cdfbaa87276d1a799554f0248b851",
    "sha256": "891435971dd51bb5e8766e99348351f553e9a501e322e4cea41f87fb4c427718"
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
    "path": "desktop/tests/shared-colors-panel.e2e.js",
    "gitBlob": "321f104e906530c678ae0a5270ecd70a92cf8577",
    "sha256": "816556668d9ea480a793943dd1d79904ccbbc5d366fa55fe8ef3fca53cf8803c"
  },
  {
    "path": "desktop/tests/text-tool-dom.test.js",
    "gitBlob": "a2215054b62c982a3f020c7afd37a88619727c79",
    "sha256": "b834eea361e630344775855c275ea276d3a2fbf6cc6fe6a253b75a0c519a819c"
  },
  {
    "path": "desktop/tests/ui-icons.test.js",
    "gitBlob": "de1676399b094cf693c413c7c38b02c679af2925",
    "sha256": "9503a9ae34db83f5b2c731c7605a389dea3e65ed06a4406e8c37ed473a8c421a"
  },
  {
    "path": "desktop/tests/ui-review-captures.e2e.js",
    "gitBlob": "73846ecd3566f0f23e9b4d7e794fa751be9a4d58",
    "sha256": "80a6444e50f9d38d2ecd8c634d8d12a8379ab7281d1f1fdbaf3a00f7489c8101"
  },
  {
    "path": "desktop/tests/workspace-panels.e2e.js",
    "gitBlob": "9685394ff3d4c32c6b9f04a7dac7b8c3b9ee69de",
    "sha256": "0046534586adfc7b5f79cb9ec925de2d0738c4ee1ace70db2a9309b7a9c374ba"
  },
  {
    "path": "desktop/tests/workspace-panels.test.js",
    "gitBlob": "8578ad837716fc7f8a9888e203e344814df03157",
    "sha256": "d4a4374382ec5d6af636f989f805db207a3eedfa6132923d308919b24f291239"
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
