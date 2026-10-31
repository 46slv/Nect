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
    "path": "desktop/src/boundary-controls.js",
    "gitBlob": "3d8ab4296dd49d197b3de5bab7028cd091928c26",
    "sha256": "0fa6c904db7eac0719387324192ac53e719b5895adc0facad824c3d3a33b3f60"
  },
  {
    "path": "desktop/src/command-menus.js",
    "gitBlob": "3b22e569e199828271e5028476b0b4dd8b77a81a",
    "sha256": "7083feba980e1a9ac8c7095be16ee9d92effbecbe11e22b41246cd607f0659e0"
  },
  {
    "path": "desktop/src/composition-view-state.js",
    "gitBlob": "12793f61bf65b740ef92459e37ee8752cb6a83de",
    "sha256": "0f064806c7de0657c96ab627f5fc55c5a6f1dce54edf3fa09f62f08255019b8a"
  },
  {
    "path": "desktop/src/main.js",
    "gitBlob": "a537b307bcf352fa533474b0b410644d5bacedce",
    "sha256": "e0aed73c5a843ed5d4609c89c80d2cf71f1e36b6bf566d35aa96d5bdbf79b97a"
  },
  {
    "path": "desktop/src/style.css",
    "gitBlob": "3768e58a171693a9640664f30e334d58a16ad77f",
    "sha256": "cc26320a33076867ef2acc82c2a6e56e40ba9cf208d6c8a139a1683518141f3d"
  },
  {
    "path": "desktop/tests/artboard-units.e2e.js",
    "gitBlob": "173fc8de5180f96adeb2465468b5821051e4ba8e",
    "sha256": "471b6c2766ed07c33caeeb5ce9864497cc6e50ce7e1283d86e4c62fb4487b808"
  },
  {
    "path": "desktop/tests/artwork-alignment.e2e.js",
    "gitBlob": "f735e494865c64e05f2fcf0dad5650ca208f7338",
    "sha256": "f7d92abd5b5522ea2367a435d4ef83eb406c53934b6dcbefa5ed8f1935a51bcc"
  },
  {
    "path": "desktop/tests/artwork-distribution.e2e.js",
    "gitBlob": "b3adeef4df4026ca8f4f1dd90834a4f77aad61af",
    "sha256": "78395eca5971bcd7c3725d8430517d07cc847753d2a76d31f521807593334607"
  },
  {
    "path": "desktop/tests/artwork-gap-distribution.e2e.js",
    "gitBlob": "4b46bbfcd3c7943632d277685ae90c4f42765ddf",
    "sha256": "fc84b08a9f0f4654fa2a13f1cd6b523b9f6039f929a92be643532e6b4f9c8fb0"
  },
  {
    "path": "desktop/tests/artwork-ordering-dom.test.js",
    "gitBlob": "d9bcdeab1fca81727fc78cca20b74f5c729063ad",
    "sha256": "398ce19c541944a558c90f9edf4274ce7a61cc567f8d466874f428aebd97b002"
  },
  {
    "path": "desktop/tests/artwork-snap-dom.test.js",
    "gitBlob": "f290c045227156483c2265677504b735b9ed72cb",
    "sha256": "f9929a2201bbfda6e1dadf443de3a6179e0ffcc74c041470cc9160d7a4f55e90"
  },
  {
    "path": "desktop/tests/asset-drag.e2e.js",
    "gitBlob": "b7b79df2205487ce8adf14cd4b3424762e2f8184",
    "sha256": "1ad7847f324182a8e2c6a1e92549160cbdcfc0be4433082915a4b369977641cb"
  },
  {
    "path": "desktop/tests/asset-library.e2e.js",
    "gitBlob": "f6018a2e70dc53a95c9d3d03005697f9adfd3081",
    "sha256": "3a5071527fdad20236b750042d86759bbdbdaa1e06dcc638d458d7cc3aba3483"
  },
  {
    "path": "desktop/tests/axis-drag.e2e.js",
    "gitBlob": "98ce7e17f0076e679e1798bd877643a37b58f38a",
    "sha256": "9502a296773e3307a2d64a16c40f5eca2412cf07b1b751123f0856964cd06342"
  },
  {
    "path": "desktop/tests/command-menus.test.js",
    "gitBlob": "f5623af4bfbf14cfd037c50363d04c721f243cb8",
    "sha256": "2fe6e19d14443236d10474f3539d14ea9421cc76bc38019395d0fcfa3c871c53"
  },
  {
    "path": "desktop/tests/composition-view-state.test.js",
    "gitBlob": "8191de301bb1158508130e31c441a73ecfd8ee76",
    "sha256": "6a08e9fa1ffac7e3e75b3448a82acfdc1e709cd37f48d39e6cfb3e961a828945"
  },
  {
    "path": "desktop/tests/editor.e2e.js",
    "gitBlob": "ae214334b07fd6c61ee259aa87ce4b5fe9f36072",
    "sha256": "aa3e5b716f7380380f1fb01d3021f9c5e074c91993f58cad33571dd8068c0c90"
  },
  {
    "path": "desktop/tests/guides-dom.test.js",
    "gitBlob": "b8456260b11ac621c42d710eb44fb733c45d263b",
    "sha256": "4b3ff28495799c708b38ff85fe602735e1bb892cb0de2640771bb0b5884d73c8"
  },
  {
    "path": "desktop/tests/harmony-wheel-panel.e2e.js",
    "gitBlob": "177d887c83f80250239381142b86d30d5e4ae886",
    "sha256": "8244048433286e05bdcb75bd9162bfefa621228d29f9a3ac7808dc49267f9361"
  },
  {
    "path": "desktop/tests/helpers/artwork-panels.js",
    "gitBlob": "3d7762cec1c7e031d82af22c40caa1cedc3e6878",
    "sha256": "9b39cd5164ad006fc4b2a18a39e809ce9264952d74d613b428666b4ca0152ef4"
  },
  {
    "path": "desktop/tests/node-parameter-feedback-dom.test.js",
    "gitBlob": "d0f386b554d95e41109e41f35de346d535c3e59e",
    "sha256": "7e7a97dd83726c9c232cf2e7e9bc27c8a9ab045cb6b8edad4ab9e3190a000f85"
  },
  {
    "path": "desktop/tests/polygon-star.e2e.js",
    "gitBlob": "1790554795a4b5e4f1c2cb064bed521d7b8c5cda",
    "sha256": "a33bb5bd8755f19e5a63f960fbc21ad225c33f0d2afc6ce35025705c2206cc49"
  },
  {
    "path": "desktop/tests/private-font-flow-dom.test.js",
    "gitBlob": "0a73eaced47b27894e5a853abd3e79fa184807fb",
    "sha256": "a1952b477f9b81a871c0ce19f3aa9e8204e74ec39c7b66c1eb4b79a79f56a637"
  },
  {
    "path": "desktop/tests/shared-colors-panel.e2e.js",
    "gitBlob": "ec3df2214ce52397508bf4956f618b30c9c2eb03",
    "sha256": "811042532084498b44d72439e60ad2a47836b0da27fdad9dbddd5adb9b0d0065"
  },
  {
    "path": "desktop/tests/system-font.e2e.js",
    "gitBlob": "5185aab99271416f55d3e5836cbb573492dea9f6",
    "sha256": "cf09a6aae98ed7113858fb523a8f11337699db323d50ece814e59b44b5cfa909"
  },
  {
    "path": "desktop/tests/text-wrap.e2e.js",
    "gitBlob": "ba841ce0d3ed27e514ebb4c170b810b64b29ee69",
    "sha256": "a07b0e00f6d8d32cb1555276b363dfd4bcd796908cbe85231595bb89463a28c6"
  },
  {
    "path": "desktop/tests/view-navigation.e2e.js",
    "gitBlob": "de903277defabf45177e29d9762f051739eb3154",
    "sha256": "29a25e6152600b1784eeefcaf2ffb24310171b1a4f88cb42cac5bd4e1069d7cd"
  },
  {
    "path": "desktop/tests/workspace-panels.e2e.js",
    "gitBlob": "1525a5f081a0bbd68c2cc8ad4aa5c14702a8a322",
    "sha256": "2e40b33f7adc52b3aada2f53d0c92d47f38a518c436c725c697e99ff57932468"
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
