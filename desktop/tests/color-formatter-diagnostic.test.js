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
    'desktop/README.md',
    'e554271252e172f1044bff48c4379709e0843e5c',
  ],
  [
    'desktop/src/harmony-wheel-panel.js',
    'f0744e04d13d24fa24d5f8523a319da0b7bf5a12',
  ],
  [
    'desktop/src/color-slots.js',
    'a0545f1d51779497274756b00357262d9d8d40ef',
  ],
  [
    'desktop/src/color-range-edit.js',
    'e734784ff3358895498871eebc28d0d53f434352',
  ],
  [
    'desktop/src/core.js',
    '67db38a0e141b9e3bb888702cb4af501f20bd032',
  ],
  [
    'desktop/src/style.css',
    '7822edfcb3f5c8ab89405cc8bb5e37a21f6f46b2',
  ],
  [
    'desktop/src/auto-chroma-panel.js',
    'c5cfbdc6e12497f11f1669a0908e451941999aa8',
  ],
  [
    'desktop/src/shared-colors-panel.js',
    'e27cf4b91a4e2368692ce6956af415d930410746',
  ],
  [
    'desktop/src/color-coverage-view.js',
    'd583bb6307401a93bf591527ecbb132fed4adc71',
  ],
  [
    'desktop/src/color-draft-scrub.js',
    'd6eb8fc7b1a3491f2d9670c7042bf6e8a0b75ffc',
  ],
  [
    'desktop/src/workspace-panels.js',
    '39a4fbb11d5c6704d90710d8f5e01c6737e43fe6',
  ],
  [
    'desktop/src/live-color-edit.js',
    '16825ea1fb533a61c2694ca72317881b7f5e7367',
  ],
  [
    'desktop/src/main.js',
    '726a9ce52f503f9edad75286b138594e92bcd1c1',
  ],
  [
    'desktop/docs/live-color-panel.md',
    'aade62227d13ee87edd14ce7c76716af96e4103b',
  ],
  [
    'desktop/tests/live-color-edit.test.js',
    '326aff7fc301ce20e6f11e62c9a50279a8b046be',
  ],
  [
    'desktop/tests/workspace-panels.test.js',
    'ad37e45fea363ede4b4a886b8206cb3827c4b0c1',
  ],
  [
    'desktop/tests/ui-review-captures.e2e.js',
    'ec464de77cc0056b90ee3dce50891fa87c9330f1',
  ],
  [
    'desktop/tests/color-spread-design.test.js',
    '84255e2bc4a59ede15f7484fc836b15a99f4a46f',
  ],
  [
    'desktop/tests/harmony-app-dispatch-dom.test.js',
    'e770d33a92d124896ac354fd0aedbd5f4a65a0c6',
  ],
  [
    'desktop/tests/workspace-panels.e2e.js',
    'd1b7b78afabfb9d0c811a5bff613a311e8ffe679',
  ],
  [
    'desktop/tests/shared-colors-panel.e2e.js',
    '2a3c716bf50d1d9163dbe151d7a47411615eb8ce',
  ],
  [
    'desktop/tests/color-range-edit.test.js',
    'e09b8e849b0a23f092eec695de795178974cd1d1',
  ],
  [
    'desktop/tests/color-slot-gesture.test.js',
    'efff1cb8b8bed25b4eea609346cdf042d1bfcd56',
  ],
  [
    'desktop/tests/harmony-wheel-panel.e2e.js',
    '265c4574c9ea266bdb16e445376cfd8043aa0f0d',
  ],
  [
    'desktop/tests/color-draft-scrub.test.js',
    '4a9806f6d6c9e814f2928027cfa68593a1bab2df',
  ],
  [
    'desktop/tests/harmony-wheel-panel.test.js',
    'a18f18cce624795c61aec61da77abedbf3024cad',
  ],
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

