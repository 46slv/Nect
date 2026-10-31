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
    "path": "desktop/docs/workspace-drawers.md",
    "gitBlob": "55b286e6447976d8ba7a67ada239d8822343bb49",
    "sha256": "9b8c9c697244b5183c915a3974f031fc320da7bcd0fd56880e107434fbabad27"
  },
  {
    "path": "desktop/scripts/browser-suite-runner.mjs",
    "gitBlob": "f9978c101bd5d4349d71e5aba46293fe233c376c",
    "sha256": "f4edbc85f2227a065156f8516c4a52a84b2949b477286593e5538bf960ad0fe8"
  },
  {
    "path": "desktop/src/asset-library.js",
    "gitBlob": "2b56032d8ff9a428e4deb4f85e700bea855cf16b",
    "sha256": "b80283c22ca625c078a286f7999d647b44c2bd8ff5f068780bcc5c44e5d89712"
  },
  {
    "path": "desktop/src/font-choice-ui.js",
    "gitBlob": "63962676bf52076b4d347e805041bf4a19fd097b",
    "sha256": "2d8825528a2268ef35d25ec0837dc4d1e555d49b0c3eb3aa571f888933fe5afc"
  },
  {
    "path": "desktop/src/main.js",
    "gitBlob": "b7e2bdd253f1c51e25b815a7bf3c6e60d851b126",
    "sha256": "4dc0919e357c484963706406ec3d1c85a74d5ea78db460816fe6455d80d2aef3"
  },
  {
    "path": "desktop/src/node-viewers.js",
    "gitBlob": "6855da7ed8531f6523b325a30817a1fad18ca115",
    "sha256": "6ae69cc727dd94a0d67cc046093ab468a5eadcdeb0aa6bc88b3b77eac9f73d7b"
  },
  {
    "path": "desktop/src/shared-colors-panel.js",
    "gitBlob": "8d62ceb7aa81d7f82c923cb0fecc9f724bc279b5",
    "sha256": "b39f87aadb948704d3015c6915780a877312a5e155cc2dfb7de3a3ec9d75017f"
  },
  {
    "path": "desktop/src/style.css",
    "gitBlob": "340b40726fa69557f5756e98379ead09c836583c",
    "sha256": "2506a3d496ba1a4257ca6e29f33c09250f1cc9a490a8fd43f96d33b195343962"
  },
  {
    "path": "desktop/src/text-inspector-layout.js",
    "gitBlob": "f58ffcb92f4436ee8774c80f7d32485b529cb411",
    "sha256": "e745b006b290d6cbf0d6e7e59fe5f0810ae6a065cddcc3201d21f0df561346a8"
  },
  {
    "path": "desktop/src/workspace-panels.js",
    "gitBlob": "6d7468d42d764b308ea783e5a1d2f26213fc15db",
    "sha256": "2e84708d985421f7613342deb5012e186a019c5e53a270cc5a82ca0ce52233b2"
  },
  {
    "path": "desktop/tests/artboard-units.e2e.js",
    "gitBlob": "90f02072ae4f9adb763e40af937ae3bc6458d353",
    "sha256": "30d24a367b268c7cf18c3c862fa40e9453ee41d9be8f65ca755dea1a1c2c8243"
  },
  {
    "path": "desktop/tests/artwork-alignment.e2e.js",
    "gitBlob": "4dabe1d02320e7adae46defc1f7a78909d93b989",
    "sha256": "dc818a4fd77eda911c98b0b6651b752f01a9d3d5176a42b3a341807ffbec0d50"
  },
  {
    "path": "desktop/tests/artwork-deletion.e2e.js",
    "gitBlob": "bd977b931d2aea4e61f6451a288d0f4b2a6f7866",
    "sha256": "aa228635b4fabb541995444bee4142b988e21c455060e034d8c07e6abc8774b3"
  },
  {
    "path": "desktop/tests/artwork-distribution.e2e.js",
    "gitBlob": "93de302804e9a79af1e28511abc04a2d81a2c423",
    "sha256": "731cfd04cfa67d892c9221052bc1fdb4a3e20b5337691ac3592286113f590006"
  },
  {
    "path": "desktop/tests/artwork-duplication-dom.test.js",
    "gitBlob": "9ae1ffd1c05de6415211418c4378361e5ac6fbca",
    "sha256": "6e7d7085a1eaac436436480c538733daeab59d904c1ae268ffca805f20754afa"
  },
  {
    "path": "desktop/tests/artwork-duplication.e2e.js",
    "gitBlob": "15dee3f61bbd40f5cbabe467091c90abe7c3140b",
    "sha256": "80bc6ca78c7e29d95639dc1b679518d42eac246a076693b85aaee48adecb1f45"
  },
  {
    "path": "desktop/tests/artwork-gap-distribution.e2e.js",
    "gitBlob": "b0f990e30d69a05ad510964a2623db3c58adcad7",
    "sha256": "e3098d5fba24fe6163ace409f63042c7c002e76bf26631cd1f362a38ebeb7949"
  },
  {
    "path": "desktop/tests/artwork-multi-duplication.e2e.js",
    "gitBlob": "a190a1ee718af09b292c54afaf73930a514cc94e",
    "sha256": "3f2eaa5bb837384a09aa8f0f0d8bcb0c99b3c3406ae774d10e72c4ef8482e47f"
  },
  {
    "path": "desktop/tests/artwork-multi-move.e2e.js",
    "gitBlob": "afd0560d0743a6b009f29b9d659ffdf987af7dea",
    "sha256": "3aba09232a6849096b9d37a47b6aa98f5f452742e927ced8d50906a8c82c7492"
  },
  {
    "path": "desktop/tests/artwork-ordering.e2e.js",
    "gitBlob": "c80402ec6ce6c80362feed925ee4bd25408ac79e",
    "sha256": "b4b36a03b2ec1e7ebdd8581c66acf1b9d9f427da10be35973699416afa25fb3e"
  },
  {
    "path": "desktop/tests/artwork-snap.e2e.js",
    "gitBlob": "cb6d6e82a4db8220306897fb53a109520756c6f1",
    "sha256": "985c08febf0062bb135aa5b75c2edd12254f168d6bcc3ee10f18e10ecacdf786"
  },
  {
    "path": "desktop/tests/artwork-workflow.e2e.js",
    "gitBlob": "f9fd81e556d277bf9450d62878a1d85a66f46a1b",
    "sha256": "a937a2a8227460a42474b3d5f65b0e39beb5482e16ea2a6be72ca2d585c79011"
  },
  {
    "path": "desktop/tests/asset-drag.e2e.js",
    "gitBlob": "d68e2c0f1c54a7df8d740f4689ddd9976f20a115",
    "sha256": "1894b5e45f1634219115b1c7996888ba9dd931fda345285a6ab10d2614d8886b"
  },
  {
    "path": "desktop/tests/asset-library.e2e.js",
    "gitBlob": "b15d5806ccf02e17b1c2f43b05ddb792cc99db82",
    "sha256": "c9c04208e5a412e4bbf566daee3e7d345e956b1da9a1319a32571f77a6cea9f2"
  },
  {
    "path": "desktop/tests/blank-artwork.e2e.js",
    "gitBlob": "85a861892c1819bab8b8079469953ba1caa24369",
    "sha256": "72f40aab8c7c28f6398fd323dfb88e6049f08877a539aa729f395d0fdb81265c"
  },
  {
    "path": "desktop/tests/browser-suite-runner.test.js",
    "gitBlob": "12c4896175d8979d9e6494a9ccf99e0c3216d35b",
    "sha256": "e30eaeb18428030365a86b3f2ed4e236690aabafa886fb89259a72ccf86b0b23"
  },
  {
    "path": "desktop/tests/canvas-transient-panels.e2e.js",
    "gitBlob": "61dea7e7170f63f7ef4efc81ac08b71ac6899557",
    "sha256": "d295879c8685be79dde2b67c9daccf7c49e9b301432d97b24e977b405791f1c2"
  },
  {
    "path": "desktop/tests/editor-names-bypass.e2e.js",
    "gitBlob": "360d750ae344c265f2361c6c6210d89879a551c7",
    "sha256": "6aba14fb2daa14ccafe4125cd4e5b166ee30ba6af1c498f623be702345e1f7f3"
  },
  {
    "path": "desktop/tests/editor.e2e.js",
    "gitBlob": "edba3ba6c0b825885491c7db19aeb060d99e00e3",
    "sha256": "8ad251e96eebd9cf9123188032063fe41f3138e94a48f91df02eec4012b55ea1"
  },
  {
    "path": "desktop/tests/font-choice-ui.test.js",
    "gitBlob": "ee9d42668516f01a4579392da7534ed5a2908888",
    "sha256": "2a8ba826f3b70ca30dbb40ce02f1270560176ced0e7fb9a9c49c05634a5963ee"
  },
  {
    "path": "desktop/tests/graph-navigation.e2e.js",
    "gitBlob": "535fe55fb4e251df38788e26511106891eb957ec",
    "sha256": "c9e8f4369d661e66de5b84dfb9ff04b9c51e601cacf76b0b185b3e664d42bd6b"
  },
  {
    "path": "desktop/tests/harmony-wheel-panel.e2e.js",
    "gitBlob": "8bd444e01f3b533cd5cc0cdd887e256df2016355",
    "sha256": "a771b54338c120ee700bd47d71ae910e48579c3ce77b7279406d86a6a0c0964b"
  },
  {
    "path": "desktop/tests/helpers/workspace-panels.js",
    "gitBlob": "fb5a077caa2e64f13f04264554a64d52004125c7",
    "sha256": "55446fe86e1f452995144a8cb88aa426deab077fea245b92c0127915fbf0e1ae"
  },
  {
    "path": "desktop/tests/images.e2e.js",
    "gitBlob": "1299053aa3c8c76e136e19b1329e4cf5fb9b2fd4",
    "sha256": "ddf13129d2fb7a9bf71d11600f3d2ca34f8a07544dd13bb3e37d132145c6b4db"
  },
  {
    "path": "desktop/tests/join-stroke.e2e.js",
    "gitBlob": "fd6fb48a982ad0305d01de83d1ecf52c7f71535c",
    "sha256": "c706d708137d30424a4b5c2723ea44783050548a5c95e0de608b814f45e2c7fe"
  },
  {
    "path": "desktop/tests/layer-duplicate-expression-refs.e2e.js",
    "gitBlob": "456c738a69c521f4a04bdfb33d3be6c61c09c13c",
    "sha256": "0498eca1891feb4c4ef54f10d71de31c0a55547dc95e29b79da871bf6811e93d"
  },
  {
    "path": "desktop/tests/layout-guides.e2e.js",
    "gitBlob": "93d3fa4735260a317b63b2888d4ff20efd00dd3e",
    "sha256": "b3200b2cb99270a9bce1a18602a12eef60b2bbae5ae0e166db614ed0fa9052ec"
  },
  {
    "path": "desktop/tests/pen-artwork.e2e.js",
    "gitBlob": "b0cfe72b70add227ff5154afb41e0a70236d6de6",
    "sha256": "9429dbfd603544941aec576b1f0a506b57a0a124480da95af9d4a8fe9828bd84"
  },
  {
    "path": "desktop/tests/polygon-star.e2e.js",
    "gitBlob": "358956f2c35b3d9c0f80f9a34606e987d5e3507e",
    "sha256": "890dd70215dc88681f36373cdbbe6834b263d2e4b12d0b2a3a529560bafd9e68"
  },
  {
    "path": "desktop/tests/private-font-flow-dom.test.js",
    "gitBlob": "9b4d4d6a20f7e3de8f264308b6830f88bc4a691a",
    "sha256": "1e180d0d87706f5ef494a6757bd2b152ec826ba57b319351e8b281cfc8b16b6e"
  },
  {
    "path": "desktop/tests/private-font-flow.e2e.js",
    "gitBlob": "0e7414a5d0f03c296d834f85bd42eb0f0a26bd65",
    "sha256": "0941973c5a681eaca30ec31a50b4df7b75f4ff834b5c557e247addbb1173540a"
  },
  {
    "path": "desktop/tests/project-file-atomicity.js",
    "gitBlob": "a8e28bc1e907f023aeed98af6c90c13885237f29",
    "sha256": "fdc74a32cc0ec05d938d09ba62c6389b941e88493797b0e8ee60081eca415a1e"
  },
  {
    "path": "desktop/tests/shared-colors-panel.e2e.js",
    "gitBlob": "807bde42e9a793879f25cca18b7c35e8728fde3a",
    "sha256": "8acfaaa2395e30004ac76804ede2fb31f733f07e4c23c8e3c75bc6d5007a5ca0"
  },
  {
    "path": "desktop/tests/system-font.e2e.js",
    "gitBlob": "15faa50e57a8ad2ce88eda93b651aec59aeb7793",
    "sha256": "7067a16cdfdd6d14148a5ed0d014c6603bcd0213ad32c7e96cf982568e756b25"
  },
  {
    "path": "desktop/tests/text-inspector-layout.test.js",
    "gitBlob": "eac1d507b359a5f2a1e259e80dea8dbeb651db30",
    "sha256": "09341a54f98fb085935e5224a0543c1ba74d940ec5e8fadf657268e3b1db4158"
  },
  {
    "path": "desktop/tests/text-trial-feedback.e2e.js",
    "gitBlob": "5b6cd2b3020b8aec0cc0fd99fd2de1871c2f14b3",
    "sha256": "e9f347f3d401dc8f158de6ce3c5035cc086fd9b96d80d41e874da244c3fe11cb"
  },
  {
    "path": "desktop/tests/text-wrap.e2e.js",
    "gitBlob": "c2627d10a9183f0fc55ec78366cef5f3a8cc8bc5",
    "sha256": "fdd0244932faad4e00269284094160be7dc9b1cd8c828d739016ecf0dcb8c959"
  },
  {
    "path": "desktop/tests/text.e2e.js",
    "gitBlob": "a8de85e36dd7f09fda48bed75780f67aeb72345d",
    "sha256": "ff4757d0fb54a2716facdafea886821ceeb1fc3a5399596d9af7dc0448263c95"
  },
  {
    "path": "desktop/tests/transient-panel-contract.test.js",
    "gitBlob": "7ed701a92af4d2db04dd7d2e47130cbb19866f1e",
    "sha256": "a873615615722e3f05beb5b2046b8d439db56f21538275d62062067f653d77af"
  },
  {
    "path": "desktop/tests/ui-font-contract.test.js",
    "gitBlob": "3d764b15d4d9f088b96e089756619aa8851b062c",
    "sha256": "a51d62d084c1cdfb813adfc981bca836ff25ad4a71d898a077f667410d139ec7"
  },
  {
    "path": "desktop/tests/ui-font.e2e.js",
    "gitBlob": "a09e678c9f0b09e904aed1f9478ec90b4cdf4293",
    "sha256": "613cca281694f5dfc9324e9442a69850e0b1f36cb836bd4fca6a7a966158ce85"
  },
  {
    "path": "desktop/tests/ui-review-captures.e2e.js",
    "gitBlob": "2c080c02c25b49b8450ba0cd6d4f1c62379fed6e",
    "sha256": "a674d2995e1024fd89a8aa3e6617bc9795081f2c6676d7bc2dd2bf51d6e0abf8"
  },
  {
    "path": "desktop/tests/unified-font-picker.e2e.js",
    "gitBlob": "45611da4233215ff9fc2da0a284dfb4b578954fc",
    "sha256": "9a636db36ea56753c1a94e1153c78ddc703a98f930987e1b85fabe0c0dac7345"
  },
  {
    "path": "desktop/tests/workspace-panels.e2e.js",
    "gitBlob": "3426ef5ad1aa62dae6217b79dfa558b272cd81f6",
    "sha256": "a708e3db2a6b21c48dab38f0da395010d4e7d4f0560ea4d950e454faab8a4c16"
  },
  {
    "path": "desktop/tests/workspace-panels.test.js",
    "gitBlob": "2c6620abe00a21167746dafacc287f331339e56f",
    "sha256": "ac3419d0453e1a4f5b3311e2ae089ad989d679dc7801b7d0548024e213f111be"
  }
];
const root = fileURLToPath(new URL('../../', import.meta.url));
const hash = (value) => createHash('sha256').update(value).digest('hex');
const blob = (value) => createHash('sha1').update(`blob ${Buffer.byteLength(value)}\0`).update(value).digest('hex');

test('capture installed formatters for the frozen workspace candidate without source writes', { skip: !process.env.CI }, async () => {
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
  await writeFile(resolve(directory, 'workflow-workspace-formatter.json'), JSON.stringify(receipt));
  assert.deepEqual(receipt.errors, []);
});
