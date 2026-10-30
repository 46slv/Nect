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
    "gitBlob": "19c9f752e90849d2a09682f738aee7ad7bf3da6c",
    "sha256": "db2b415f3f6a7f94a9bc9f77601b26eac22a4aa8245aab2ee21c269ce89177f8"
  },
  {
    "path": "desktop/scripts/browser-suite-runner.mjs",
    "gitBlob": "f9978c101bd5d4349d71e5aba46293fe233c376c",
    "sha256": "f4edbc85f2227a065156f8516c4a52a84b2949b477286593e5538bf960ad0fe8"
  },
  {
    "path": "desktop/src/asset-library.js",
    "gitBlob": "7ff15c2f06f6e1209ef990d9dce0afc69afcdf74",
    "sha256": "ea65ee23dd46f4925781b26a1d6c7eadfbadd8cf88a249ce4d26225637d59a23"
  },
  {
    "path": "desktop/src/font-choice-ui.js",
    "gitBlob": "63962676bf52076b4d347e805041bf4a19fd097b",
    "sha256": "2d8825528a2268ef35d25ec0837dc4d1e555d49b0c3eb3aa571f888933fe5afc"
  },
  {
    "path": "desktop/src/main.js",
    "gitBlob": "e920e906fc87276399fe6231fdf6316bf47931ab",
    "sha256": "22096e698d749ab643911c05b3122b23f0a36facfac6115e7bd07266874dd1a0"
  },
  {
    "path": "desktop/src/shared-colors-panel.js",
    "gitBlob": "8d62ceb7aa81d7f82c923cb0fecc9f724bc279b5",
    "sha256": "b39f87aadb948704d3015c6915780a877312a5e155cc2dfb7de3a3ec9d75017f"
  },
  {
    "path": "desktop/src/style.css",
    "gitBlob": "0562362379492d73cb57b2f57d2dbbbee7d6826b",
    "sha256": "cbf22d4a064cf56a3f5832fab2ac38050937cbea3bf02ac7307e0fb1cc383e80"
  },
  {
    "path": "desktop/src/text-inspector-layout.js",
    "gitBlob": "1b3b14c0bb1e2319e8d03996cab8deeb4d47a7df",
    "sha256": "56759187f989fd76284403e8ba0bc4707d3c8314212786e0602246ff283bdc9c"
  },
  {
    "path": "desktop/src/workspace-panels.js",
    "gitBlob": "2102bc60a72001bd2802af5da336e50cd5b6b23e",
    "sha256": "266d299ebfd9301ac98043df8052a885df72c885111998550f0fa3faf1b97529"
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
    "gitBlob": "346fb72c959ae5ba50af5065df11f3aabfac7847",
    "sha256": "7843af85bc37d618da69fa44c8dcd62c2cdfc28aa519ea0482c87b8b8150c711"
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
    "gitBlob": "ee256762c82d8a0c632cd5c4d0328b296e42be18",
    "sha256": "dfd3242204fa3fe59175ec4193715b9b1a3fa21ed57178a76e6813653b865800"
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
    "gitBlob": "a8f145720a634f73283fb2337007bfb0a53c3932",
    "sha256": "a6401c1b6216ceaea94b54086afd408bef87d5f4a8d6dce69c4b72bc1e9d9869"
  },
  {
    "path": "desktop/tests/asset-drag.e2e.js",
    "gitBlob": "3939a2a9243219ef9fa7ff2672bbf4c61ce9551f",
    "sha256": "dcc7a699f5b931a27424c6137a1cb6da0aa5f8c0ffd8c7c423f8a8cd8270ded2"
  },
  {
    "path": "desktop/tests/blank-artwork.e2e.js",
    "gitBlob": "a29c3c71099881dd11d3e5ae00a8954242d86e43",
    "sha256": "cd1034bc3826e42749a5d33a61c968b718d750b03a7056da895bd86e76613a8e"
  },
  {
    "path": "desktop/tests/browser-suite-runner.test.js",
    "gitBlob": "37a3169ada77505f4fa2e12ed8b414f4ce57e41e",
    "sha256": "47ea87c2c37b87530b9a6ed9c41f37099eb1364ac2f4e9346158a5fefb49ca09"
  },
  {
    "path": "desktop/tests/canvas-transient-panels.e2e.js",
    "gitBlob": "befad9ffbf8e7e4e3a72ee619258f0a4321f65ff",
    "sha256": "0a6e7bd029decc1d7557f3209231c917d6a4b3b1daa94d6211aa993495585d70"
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
    "gitBlob": "295d606dba88b1527ee1614c973a557c7829c6c8",
    "sha256": "140622cf82e78cacecd0cac526da6276df4a69934fb9cd6b21c594e4d6b0cc55"
  },
  {
    "path": "desktop/tests/harmony-wheel-panel.e2e.js",
    "gitBlob": "fcf76fb7edfe3e537a0e184e2410fb81896184fc",
    "sha256": "18db83f7b6b495b3c07dcfb1a75fb658156613da69200fa06766a39d2a41bbdd"
  },
  {
    "path": "desktop/tests/helpers/workspace-panels.js",
    "gitBlob": "c8a8e1554a9ab402144850b1025a00a16d8baec1",
    "sha256": "a25015945805b601156973dc30c7175c755bc2bef5795c30017724f67ebd7c6a"
  },
  {
    "path": "desktop/tests/images.e2e.js",
    "gitBlob": "084874b7d737222e553760946a1ff1a3eee75af2",
    "sha256": "31345d41601a5b13d6fb9b0831d83fbae5e4f29c13578672b7b6ccf9fb1a87ae"
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
    "gitBlob": "4ba2d532a86a333c70c882e007a7a13374c8a3a4",
    "sha256": "0226793d060fb83e53d5b6f7804b9b9358372ac52c9f74ec18b237caca4341dc"
  },
  {
    "path": "desktop/tests/pen-artwork.e2e.js",
    "gitBlob": "e2538d7d64a953c34a75dc71ac2be8f8689af07c",
    "sha256": "85acbe25ec0421be5875886374480da401e03ba080c0dc22dd513cf57783549b"
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
    "gitBlob": "280eee4be3985bffcc02d8c184354818857762a6",
    "sha256": "a39ead6350a7ff6c71c026b692e948cb6ea2df8daa7c9057d5b82eb659ab46e0"
  },
  {
    "path": "desktop/tests/project-file-atomicity.js",
    "gitBlob": "a8e28bc1e907f023aeed98af6c90c13885237f29",
    "sha256": "fdc74a32cc0ec05d938d09ba62c6389b941e88493797b0e8ee60081eca415a1e"
  },
  {
    "path": "desktop/tests/shared-colors-panel.e2e.js",
    "gitBlob": "2b7cca907b4a51d416490e8a445639a3c995b4fe",
    "sha256": "6ead68ea81a44de207aa9980b8a1a4ef598e56a07b9c389b2f8f85491e8e2f6d"
  },
  {
    "path": "desktop/tests/system-font.e2e.js",
    "gitBlob": "15faa50e57a8ad2ce88eda93b651aec59aeb7793",
    "sha256": "7067a16cdfdd6d14148a5ed0d014c6603bcd0213ad32c7e96cf982568e756b25"
  },
  {
    "path": "desktop/tests/text-inspector-layout.test.js",
    "gitBlob": "d16260951c9fe2d5b3512c2c2ad33d7ec8868404",
    "sha256": "272d55ac81d7a41a01fa0c132016926b072e22198c7b2d504271ddcbaba4c095"
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
    "gitBlob": "b3f92520fd7b97ab0b348b4d756addb03981139f",
    "sha256": "b2678e0f23d6c7f125b6ccbd6839898a594b54b56ada6d73d11c15f4d588ced5"
  },
  {
    "path": "desktop/tests/ui-font.e2e.js",
    "gitBlob": "944c666d4cd56120275ddb31d6538de979e216b1",
    "sha256": "0402386d32a86755bd9015349800ebb37e4421c6da3a87154ee2220053fd5b20"
  },
  {
    "path": "desktop/tests/unified-font-picker.e2e.js",
    "gitBlob": "27c4399c22ff3e8f2462739eee53893e782693d3",
    "sha256": "e20170e316635d2bde3ec775a4a2583563476cc6605e19f7afeb5b29df95f160"
  },
  {
    "path": "desktop/tests/workspace-panels.e2e.js",
    "gitBlob": "e6b9c672a148eb3295962880ba2093d28e1b1152",
    "sha256": "55a0e5b164c0dae966dde330e6743a4f33084c24c2c4c8a441a57e28399df240"
  },
  {
    "path": "desktop/tests/workspace-panels.test.js",
    "gitBlob": "1b122b860f7abfcc6a46d2c27f347c1504a00a2f",
    "sha256": "0401b6bbb4dbff96a003a076fd4314cd0f20f0beea522897961e77334b2c7648"
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
