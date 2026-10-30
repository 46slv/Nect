// Temporary read-only CI diagnostic. Remove before the final candidate.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import * as prettier from 'prettier';

const files = [
  [
    "desktop/docs/font-choice-core.md",
    "9f3e48a75eaf11416a67782fa0ee846f4571f9ec"
  ],
  [
    "desktop/docs/unified-font-picker.md",
    "926283159615d24556f441c33011cbbce71f09c3"
  ],
  [
    "desktop/scripts/browser-suite-runner.mjs",
    "48750be3b418e3337115da5a28b8ec9aef4d263a"
  ],
  [
    "desktop/src/core.js",
    "a78181bcc8531ea545d5026aa9c4f0e7b72b28a6"
  ],
  [
    "desktop/src/font-choice-controller.js",
    "9c9cf45e9a4ea7b45ebf4a6be1ae56154d231f17"
  ],
  [
    "desktop/src/font-choice-inspector.js",
    "6bbed3f0068cdf7a3558920044d1e3ad38c2537e"
  ],
  [
    "desktop/src/font-choice-ui.js",
    "1a64aa372c04894544770ee0b1eb7df67cb3b262"
  ],
  [
    "desktop/src/font-choice.js",
    "a2fe688d826f3c5475993b81cfa2ffed3fc863d0"
  ],
  [
    "desktop/src/main.js",
    "47b3c914d5b77c7d4d4d839b453b83626fbd4b81"
  ],
  [
    "desktop/src/text-inspector-layout.js",
    "c34ced7ff5cd76d9f67a4d56ad69927d72d6b1b9"
  ],
  [
    "desktop/tests/desktop-font-text.e2e.js",
    "6395dcde4cf4bc16ba73572a4db9507ed0321baa"
  ],
  [
    "desktop/tests/font-choice-controller.test.js",
    "30ae3a97fc77b8bf3e02b1714a89f20b9b52d4c0"
  ],
  [
    "desktop/tests/font-choice-inspector.test.js",
    "f6e49b3366ce6eb62c0d1c3abaea6db3ed51b9e7"
  ],
  [
    "desktop/tests/font-choice-ui.test.js",
    "3ccf68c748a5adfeb7b8ead5aeedefca158010ec"
  ],
  [
    "desktop/tests/font-choice.test.js",
    "5eb0011656290e7209b42a7fbbee000899925469"
  ],
  [
    "desktop/tests/font-dom.test.js",
    "e07ff56429bcdceb6c38d179862feefcb010ad89"
  ],
  [
    "desktop/tests/font-style-dom.test.js",
    "7138085a754bc1a82c1698b45607e2bad8493289"
  ],
  [
    "desktop/tests/font-switch.e2e.js",
    "a5722b9e2f083895eea5a2a3652669387d62f2ce"
  ],
  [
    "desktop/tests/os-font-outline-dom.test.js",
    "46abb2ceb1f76043b6c39e64e9d6b956bedf49d0"
  ],
  [
    "desktop/tests/os-font-outline.e2e.js",
    "631bfa0c12675be0593c2671f7e8f813b99fae26"
  ],
  [
    "desktop/tests/os-font-replacement.test.js",
    "bc7c712c4a2ce06ac20194f9b83a8b6f8385f9fb"
  ],
  [
    "desktop/tests/rich-text-dom.test.js",
    "773db2dbf0bf8fb4419e93f57c89f5125ff1b7ee"
  ],
  [
    "desktop/tests/rich-text.e2e.js",
    "d812f0413ac4a818fc79d9af0b522dc99da169c2"
  ],
  [
    "desktop/tests/system-font-dom.test.js",
    "ad952d1f020fd0d976ff614bd3dae599423a66bd"
  ],
  [
    "desktop/tests/system-font.e2e.js",
    "9881c1f395c23a1f251a3775c11c5893cd24cb46"
  ],
  [
    "desktop/tests/text-inspector-layout.e2e.js",
    "e6a9471c45b0970b43eb04593430b450f8eb8e0f"
  ],
  [
    "desktop/tests/text-inspector-layout.test.js",
    "ef4e72453fb8309324f74e5803609c3072fad6dc"
  ],
  [
    "desktop/tests/unified-font-picker-dom.test.js",
    "9efd0df841b62727605561dcf9a7b63fcd34bc9d"
  ],
  [
    "desktop/tests/unified-font-picker.e2e.js",
    "bce92a766cc31035879f0d9304a065b5d74a6375"
  ],
  [
    "desktop/tests/variable-font-ui.test.js",
    "de2836816300f9f16a16bfd8cc194cf3834fdadb"
  ]
];
const repo = fileURLToPath(new URL('../../', import.meta.url));
const desktop = resolve(repo, 'desktop');
const hash = (bytes) => createHash('sha256').update(bytes).digest('hex');
const git = (...args) => execFileSync('git', args, { cwd: repo });
const gitText = (...args) => git(...args).toString('utf8').trim();
const blob = (bytes) =>
  createHash('sha1')
    .update(Buffer.from(`blob ${bytes.length}\0`))
    .update(bytes)
    .digest('hex');

test('temporary allowlisted in-memory formatting evidence', {
  skip: process.env.GITHUB_ACTIONS !== 'true',
}, async () => {
  const receipt = {
    diagnosticOnly: true,
    prettierVersion: prettier.version,
    githubSha: process.env.GITHUB_SHA,
    runId: process.env.GITHUB_RUN_ID,
    runAttempt: process.env.GITHUB_RUN_ATTEMPT,
    checkoutCommit: gitText('rev-parse', 'HEAD'),
    checkoutTree: gitText('rev-parse', 'HEAD^{tree}'),
    files: [],
    errors: [],
  };
  if (prettier.version !== '3.6.2') receipt.errors.push('Unexpected pinned Prettier version');
  else {
    for (const [path, expectedBlob] of files) {
      try {
        const file = resolve(repo, path);
        const original = await readFile(file);
        const canonical = git('show', `HEAD:${path}`);
        assert.equal(blob(canonical), expectedBlob, `Unexpected source: ${path}`);
        const config = await prettier.resolveConfig(file, { editorconfig: true, useCache: false });
        const info = await prettier.getFileInfo(file, {
          ignorePath: [resolve(desktop, '.gitignore'), resolve(desktop, '.prettierignore')],
          withNodeModules: false,
        });
        assert.equal(info.ignored, false, `Unexpected ignored source: ${path}`);
        const text = original.toString('utf8');
        const formattedText = await prettier.format(text, { ...config, filepath: file });
        assert.deepEqual(await readFile(file), original, `Source changed: ${path}`);
        receipt.files.push({
          path,
          expectedBlob,
          observedBlob: blob(canonical),
          worktreeBlob: blob(original),
          sourceSha256: hash(canonical),
          worktreeSha256: hash(original),
          formattedSha256: hash(Buffer.from(formattedText)),
          formattedBlob: blob(Buffer.from(formattedText)),
          config,
          ...info,
          needsFormat: formattedText !== text,
          crlf: (text.match(/\r\n/g) || []).length,
          lf: (text.match(/\n/g) || []).length,
          formattedText,
        });
      } catch (error) {
        receipt.errors.push({ path, message: error.message });
      }
    }
  }
  const out = resolve(desktop, 'test-results');
  await mkdir(out, { recursive: true });
  await writeFile(resolve(out, 'workflow-formatter-diagnostic.json'), JSON.stringify(receipt, null, 2));
  assert.deepEqual(receipt.errors, []);
  assert.equal(receipt.files.length, files.length);
});
