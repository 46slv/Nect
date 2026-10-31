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
    "gitBlob": "464bdf5b35107b7bcc7edf31871ad8cc051b9224",
    "sha256": "c72ccb6ce2b6ed592e4be1ab0d299d5d8de7d7991ea196490d1c6f511e7c180b"
  },
  {
    "path": "desktop/src/style.css",
    "gitBlob": "f37ccf1d0e684ee2e460c572fe7997db951148ff",
    "sha256": "1aaf50c291c3799136f329cb817411a836c9ebdc2cc3eebc19b66059f850e629"
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
    "gitBlob": "2ff551d4301c19335be30ca5bb78090dfa148ec1",
    "sha256": "fbbdb8e94a846cc2e42611a654d29dd8736ec6bc028ece34c7b6bb9ca8358842"
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
    "path": "desktop/tests/command-download.test.js",
    "gitBlob": "63ac63bd5c1ee857239b1d9eb2a42fc47bb69674",
    "sha256": "4fe53f999e9c7fd3974a4d25afb4275a0b51770625753fa4fb2eb34f0963201b"
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
    "path": "desktop/tests/command-session.e2e.js",
    "gitBlob": "2bdc091c90a26c8393a0d9661a6888dd18a5dc8d",
    "sha256": "1067ebb5a89149fe2c204934d8625d7e858712a5b949e7d10f3dea93648664b5"
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
    "gitBlob": "f3095ba24596273ff24bb15ac7fb9d31d73d1c8d",
    "sha256": "70224dfe584c90bdc27336417ee4d7f984b12900ab049b715f976a232647853f"
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
    "gitBlob": "c19373672ac74aeecda86514cc91b1e7b3780467",
    "sha256": "84b16a56b92b18288eb2c3242dc250f05b50e0f4cc449246ac9f5fc40fd1927f"
  },
  {
    "path": "desktop/tests/workspace-panels.e2e.js",
    "gitBlob": "e6ed2a73e76c5c03bc2dc876b75468d67a0222e6",
    "sha256": "2d101b7091317e87c01d1d6647f872acd5db44f5d798385a4675be6b86c84a91"
  },
  {
    "path": "desktop/tests/workspace-panels.test.js",
    "gitBlob": "1bdb9fff079b5c295b1dbab5b44d773dad620fa5",
    "sha256": "bb29e3bf1f4d931c59a5e7fc3cdc453bbb092d9fdb11f2cd394463b29a8075ea"
  },
  {
    "path": "desktop/src/ui-icons.js",
    "gitBlob": "9d0935056b19c33312ad6d488e0a5ab2e5fd1600",
    "sha256": "d1faec6e5281faa203bf9415257b04a3e13996e2bd970d2e40f2489d0d948379"
  },
  {
    "path": "desktop/tests/ui-icons.test.js",
    "gitBlob": "94e01a6d5291c97fd0a4ac880ece89fd77623580",
    "sha256": "fd3e56ff66577131f543552da0e8f0f80a7d8010d27345afc486f97413b4dad9"
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
