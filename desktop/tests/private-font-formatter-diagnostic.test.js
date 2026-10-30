// Temporary read-only CI diagnostic. Remove before final acceptance.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { execFileSync, spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import * as prettier from 'prettier';

const files = [
  {
    "path": "desktop/docs/private-font-files.md",
    "gitBlob": "08dd1d4c13a4225ac1bec81bbfce7512c7cec4df",
    "sha256": "850df482cb133b7dac0c5b9023aa9db7bfda1bd93a0451b7197b655b0ea9dfab"
  },
  {
    "path": "desktop/scripts/browser-suite-runner.mjs",
    "gitBlob": "ff0fbed92906af1f3d346d6250c9b8d9248a390a",
    "sha256": "b4dd423fd494fe909077964e414b3eb562573268178db9bd06abf7954fe13d2b"
  },
  {
    "path": "desktop/src-tauri/src/acceptance_storage.rs",
    "gitBlob": "63d28b387e4b014e5b101c2462d00903cda8c05d",
    "sha256": "4ac4fe0eeff82b8fad88eb3d6d0b701d2c199a0f0ac384a027a638d2a00b62db"
  },
  {
    "path": "desktop/src-tauri/src/desktop_font_layout.rs",
    "gitBlob": "a13c87c17a32ede995dfdd928cb55516417e24dd",
    "sha256": "7501298131f789fb4bff5b7b497960991d3da517ccd7c35cfab45adec2e94caa"
  },
  {
    "path": "desktop/src-tauri/src/main.rs",
    "gitBlob": "465903a1dd7dd2efcfc12e86309a3bd19e881fbf",
    "sha256": "ddb1be137c6a3b5b628cfbe93761359feabac2ee09f3f701be77eef69c2fbdbf"
  },
  {
    "path": "desktop/src-tauri/src/private_fonts.rs",
    "gitBlob": "b6f5414518db7c56df7afd996d00d13b01a4d640",
    "sha256": "258c96ec91b556805a2caea84f14b510a295f65f42a2041b1b98b5331d196262"
  },
  {
    "path": "desktop/src/core.js",
    "gitBlob": "69140ed27f4e033d8649058285abde818b7ee6dc",
    "sha256": "b45e62204a785353fbe6d7729fa0fd6b6e59bc43e5ca47e502433dddca474010"
  },
  {
    "path": "desktop/src/desktop-font-text.js",
    "gitBlob": "ac5298f51880acef0cc6b6852cd2353a4c7a35be",
    "sha256": "c2baf842288abb479387e827db0b038c7e452ef1eb6e178abc60265e76d89ad5"
  },
  {
    "path": "desktop/src/font-choice-controller.js",
    "gitBlob": "f3b863804c949c3727f7c80a6161eac9eb43c7e7",
    "sha256": "80782c134ae31374bb15b2b947a2efd8b62f2e9a5ac356495ab49c6465e13e9f"
  },
  {
    "path": "desktop/src/font-choice-inspector.js",
    "gitBlob": "074ea98161c6bf64a489b9059edc9d23a7815593",
    "sha256": "863ef952f04a95c7717c84b805565a97a8ea735d2738707a50b596a91e6de882"
  },
  {
    "path": "desktop/src/font-choice-ui.js",
    "gitBlob": "dcbf00b5fb6e70fbb77c78da1c4419804f3dcf4c",
    "sha256": "cc1109ce15d45cfcbd02a48436f3fbc559ec0700a23985d3e78d342202c556dc"
  },
  {
    "path": "desktop/src/font-choice.js",
    "gitBlob": "f50ee692db9b754e16763835564339a1f840f9fd",
    "sha256": "8e455ba1a9d415ae9d6124fd28374c09b8e1c593d3853b324cc57b4cfb512559"
  },
  {
    "path": "desktop/src/main.js",
    "gitBlob": "5d99d23d231521e0cf67c6bf0bbcaef86c5d5c7e",
    "sha256": "317772788fda5032f3c48af388b0cf9d4eb1c27b4e068295c06489d826b0d6a8"
  },
  {
    "path": "desktop/src/private-font-source.js",
    "gitBlob": "8c82389386fed42093f8918f162ae29730debac1",
    "sha256": "8de46a3cbc6e743b3f8cab662c79cf682db713f0b5934a3d8b3fa871edf58228"
  },
  {
    "path": "desktop/src/storage.js",
    "gitBlob": "0e4e382983dced2fb659e40ea3905348e6fbfa71",
    "sha256": "f77b378b47adacca85a08814eaff60e49e9b5f4809582fdc4844d724e610e64f"
  },
  {
    "path": "desktop/tests/browser-suite-runner.test.js",
    "gitBlob": "9c66643211b15caf2a1c4aabc20d2fbc744580d8",
    "sha256": "7798db3a5f9bd38bf656773dcd1b26f11d42546a53edf52d0c04daaafbded331"
  },
  {
    "path": "desktop/tests/private-font-flow-dom.test.js",
    "gitBlob": "9c4fc2aedb3ee36f0b087b2669571ad2a7b66c2f",
    "sha256": "1a7f2ba7d30470c8a806fc790ad9123c3b2935525c5eb99bc5dcf0999a2548f2"
  },
  {
    "path": "desktop/tests/private-font-flow.e2e.js",
    "gitBlob": "f4efd9d93271a9aacf2e29ad8f17c5dc92c6ce93",
    "sha256": "4cefa6281050e4a3f155ed2ba7ea65dd41315763a03c6e088628ac3ae005e458"
  },
  {
    "path": "desktop/tests/private-font-picker.test.js",
    "gitBlob": "106d118f1902fafd3e789cf643f374fe980d79d2",
    "sha256": "d0af718bccb0bc26b95fbb954cfbbf37d9b1aa64bf50ad0da76df191209041df"
  },
  {
    "path": "desktop/tests/private-font-session.test.js",
    "gitBlob": "894586f85cea183a84d9d681aa3c7325a066303b",
    "sha256": "62bc35f6f71239d1a3db7881eb902e3140b7250d32a9a8852a1b2b2148981aff"
  },
  {
    "path": "desktop/tests/private-font-source.test.js",
    "gitBlob": "4e4da10338b108933db184b0449a886c8ea9678d",
    "sha256": "c2782a1562594930ea3ffe2db362db3ec3b60a6c5e11d38a6e068e94e3fd9e89"
  }
];
const root = fileURLToPath(new URL('../../', import.meta.url));
const hash = (value) => createHash('sha256').update(value).digest('hex');
const blob = (value) => createHash('sha1').update(`blob ${Buffer.byteLength(value)}\0`).update(value).digest('hex');

test('capture installed formatters for the frozen private-font candidate without source writes', { skip: !process.env.CI }, async () => {
  const git = (...args) => execFileSync('git', args, { cwd: root, encoding: 'utf8' }).trim();
  const rustVersion = spawnSync('rustfmt', ['--version'], { encoding: 'utf8' });
  const receipt = {
    runId: process.env.GITHUB_RUN_ID,
    attempt: process.env.GITHUB_RUN_ATTEMPT,
    checkout: git('rev-parse', 'HEAD'),
    tree: git('rev-parse', 'HEAD^{tree}'),
    prettier: prettier.version,
    rustfmt: rustVersion.stdout?.trim(),
    rustfmtStatus: rustVersion.status,
    files: [], errors: [],
  };
  if (prettier.version !== '3.6.2') receipt.errors.push('Unexpected installed Prettier version');
  if (rustVersion.status !== 0) receipt.errors.push('Installed rustfmt unavailable');
  for (const expected of files) {
    try {
      const path = resolve(root, expected.path);
      assert.ok(path.startsWith(root));
      const original = await readFile(path, 'utf8');
      assert.equal(blob(original), expected.gitBlob, `Unexpected preimage: ${expected.path}`);
      assert.equal(hash(original), expected.sha256, `Unexpected bytes: ${expected.path}`);
      let formatted;
      if (expected.path.endsWith('.rs')) {
        // Stdin has no filesystem filename. skip_children also prevents module
        // discovery; the formatter receives exactly one allowlisted source.
        const result = spawnSync('rustfmt', ['--emit', 'stdout', '--edition', '2021', '--config', 'skip_children=true'], {
          input: original, encoding: 'utf8', maxBuffer: 16 * 1024 * 1024,
        });
        if (result.status !== 0) throw Error(`rustfmt failed: ${result.stderr}`);
        formatted = result.stdout;
      } else {
        formatted = await prettier.format(original, {
          ...(await prettier.resolveConfig(path)), filepath: path,
        });
      }
      receipt.files.push({ ...expected, formattedSha256: hash(formatted), formatted });
      assert.equal(await readFile(path, 'utf8'), original, 'Diagnostic mutated source');
    } catch (error) { receipt.errors.push(`${expected.path}: ${error.message}`); }
  }
  const directory = resolve(root, 'desktop/test-results');
  await mkdir(directory, { recursive: true });
  await writeFile(resolve(directory, 'workflow-private-font-formatter.json'), JSON.stringify(receipt));
  assert.deepEqual(receipt.errors, []);
});
