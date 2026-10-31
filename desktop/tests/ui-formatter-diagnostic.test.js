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
    "path": "desktop/docs/workspace-boundary-test-migration.md",
    "gitBlob": "16e9c849d11e2637f5b2fc1369fdd5209b8e79ae",
    "sha256": "04891ff717cfff89156bf4b20ffa019a2e1c3696071cb30ede62e470041a65a4"
  },
  {
    "path": "desktop/src/asset-library.js",
    "gitBlob": "d5506a23dc095e6f3a2989f73197e269cd717b57",
    "sha256": "458991450dd922cc70cf522e16dfa018a8ab59fb35f3be0f390c99c53090a9c2"
  },
  {
    "path": "desktop/src/boundary-controls.js",
    "gitBlob": "7fde2c4185bc53543370809444bd4c75f5868d2f",
    "sha256": "ffec48a3cc806541d468131919823a5fbf81aab504656818f46ce72046ee1dc6"
  },
  {
    "path": "desktop/src/build-info.js",
    "gitBlob": "b8fe1131447d9910e173ac82c45d9a7131c6b456",
    "sha256": "19f602a3f36868dcaff14601bfd81a8e6488b5517d9b8d2dcbc553230980b49c"
  },
  {
    "path": "desktop/src/command-menus.js",
    "gitBlob": "08fc0f5df72b3c56fd0ab705da2ad4d7c9cfc17a",
    "sha256": "fa2507fcdf65da66387ce0325bebe632d6c7d21f8279c5e1304f23e795c708cc"
  },
  {
    "path": "desktop/src/main.js",
    "gitBlob": "52f78cf40b8e7cff34d7faad74c289be7c5e3767",
    "sha256": "6ed7dce14f4290c8fa5f52d5179b0d2caad25ee11f7b119a1176572dcb53a408"
  },
  {
    "path": "desktop/src/style.css",
    "gitBlob": "7294f4fe239c1c19a04a903bd6a8b54e897b8af6",
    "sha256": "8f961afc770e852fe25ef6de061dae94e7f970646a71c3343c39fb2530515953"
  },
  {
    "path": "desktop/src/text-tool.js",
    "gitBlob": "d1771ff78a491a1c1a52676e8030000332b0f7d2",
    "sha256": "82900d2959efb5c4c9da8b0f54e189a19a6d3f2424b5315501fb768aae0e1400"
  },
  {
    "path": "desktop/src/ui-icons.js",
    "gitBlob": "2f0bbfe82abfff649018204c39f48dcb14732746",
    "sha256": "43dfe1017fc088a7a0749b5803aac84907ab7b9e4d2e96b6d50ad99fcc1e1a72"
  },
  {
    "path": "desktop/src/workspace-panels.js",
    "gitBlob": "dcf5f01e1fe8afd918cb126ac3ab90171bdfb96d",
    "sha256": "0ba07efca769fc016083c6e05485abbcfe03166e74413d5be03adda7387796c2"
  },
  {
    "path": "desktop/tests/angled-grid-dom.test.js",
    "gitBlob": "8453259658bc2ac194813441a4f7decd98cea47e",
    "sha256": "7244a08d661cc5d8e26bd5fb20de7882961785c48057e382f4074d8033cdce32"
  },
  {
    "path": "desktop/tests/angled-grid-png.e2e.js",
    "gitBlob": "c5a84fb1660766cde6f50179c9fe59c8951e6994",
    "sha256": "8c7ad3156d5cc4c59ce5b11f1b205bf7676550cffb1244cfc40657f84051fe91"
  },
  {
    "path": "desktop/tests/angled-grid.e2e.js",
    "gitBlob": "8276972231e65f70026f89066f2cb3b3237956c9",
    "sha256": "3f160b4884842657491028a850ebcef40d385eac49cfb436d6c3f36701be6fd3"
  },
  {
    "path": "desktop/tests/angular-gradient.e2e.js",
    "gitBlob": "e9dd579566b5a80f55a3f5cc08b243337a08451c",
    "sha256": "4ab4255da55bb7a03b67dd6db4a4729ab412e2b6a09baefdee571ff31b104a25"
  },
  {
    "path": "desktop/tests/artboard-pan-dom.test.js",
    "gitBlob": "c6608005c9b7fb06aa68891cb0a6cb80b59b3278",
    "sha256": "c771e7f4c78c541b4f5633b02e6b846342913e90a6535a0adc0ddac0ee372350"
  },
  {
    "path": "desktop/tests/artboard-pan.e2e.js",
    "gitBlob": "79773470c922d937706bd2126bb355052448c65f",
    "sha256": "8f6c08d63f3cde541d6103360f6d3ca42c9fbb9a80baddcd0d2dba43041348be"
  },
  {
    "path": "desktop/tests/artboard-units.e2e.js",
    "gitBlob": "48b0a25317ba0cceddacaaaa87f626d1c127ba83",
    "sha256": "0faece7ad40404724302d32b56810415201511a0a5b7745735fe9a10890b5fa0"
  },
  {
    "path": "desktop/tests/artwork-alignment.e2e.js",
    "gitBlob": "4c261b41338c3a787b45ada91861de5d57a7255b",
    "sha256": "fb5149d34f193bcb4bb0a3a3525df6691eb8826449f4beeb214e31178ab68c4e"
  },
  {
    "path": "desktop/tests/artwork-deletion-dom.test.js",
    "gitBlob": "e367d451694a65a1a56b2af976fb293e86f5ca80",
    "sha256": "a6d59d18c8e13523416ce465410cf2a0a7c15082336614818895e9910d5fc142"
  },
  {
    "path": "desktop/tests/artwork-distribution.e2e.js",
    "gitBlob": "aafff974b0754be489e1d058030892e3bf20461e",
    "sha256": "1fed9b0bcd513bd25c698e5e30880d61eff4c65d03351e7c0b5821b973af12f2"
  },
  {
    "path": "desktop/tests/artwork-gap-distribution.e2e.js",
    "gitBlob": "8ba754b4fe487aec3e87e81b77b9f1ca1c05774b",
    "sha256": "896e4a978e3324445e002f1f04be07f7b22839031e9a916030330b13e0ad9700"
  },
  {
    "path": "desktop/tests/artwork-multi-move-dom.test.js",
    "gitBlob": "b367f40ce72bf14dfc3611b27bf051398d1c6253",
    "sha256": "b2da9f1c41ffff8664137b518c2a319afc80b7a6caf2c3009eeb6ec947da9deb"
  },
  {
    "path": "desktop/tests/artwork-multi-move.e2e.js",
    "gitBlob": "82a3aec1d642bc7414cf008ce6112c929b9a38a6",
    "sha256": "87e28477f94e9fd8a42402ae454c3f942395a264e0f50a3362eb02e747a44575"
  },
  {
    "path": "desktop/tests/artwork-ordering-dom.test.js",
    "gitBlob": "15ffb4c7ffdb18830c15cf0434fe5495be607b05",
    "sha256": "14495d2977ce3e0d99a22a20b7d8425b9d6472c06dee73c2c5b1f01246bf502c"
  },
  {
    "path": "desktop/tests/artwork-ordering.e2e.js",
    "gitBlob": "2ff551d4301c19335be30ca5bb78090dfa148ec1",
    "sha256": "fbbdb8e94a846cc2e42611a654d29dd8736ec6bc028ece34c7b6bb9ca8358842"
  },
  {
    "path": "desktop/tests/artwork-snap-dom.test.js",
    "gitBlob": "a5f66bdb51af32e3268ede78ca956ca0672c65ae",
    "sha256": "082a71fbe60daad4b55668cfb7d78491a35e3c8d807c6a9f5762884631638c84"
  },
  {
    "path": "desktop/tests/artwork-snap.e2e.js",
    "gitBlob": "27ce3049daeab1410e482171b575c838c2e752c8",
    "sha256": "e53e5a6b182a4f32ecc70280f4db8b5f259f20dac797e37cf2be2c4b22b1c6f8"
  },
  {
    "path": "desktop/tests/artwork-workflow-dom.test.js",
    "gitBlob": "d30334fe0b8f16e9c848f1118d4d946e68132300",
    "sha256": "c7866674381d87e9b9fe21a66c55b9c116841a994ae028eed8d09de61b9b4874"
  },
  {
    "path": "desktop/tests/artwork-workflow.e2e.js",
    "gitBlob": "113caabea9b458dc159fabc5f5bf635061f58e56",
    "sha256": "7f58ecaf9955584396defa2dfccf0d2edb050453ad7dce8c5903aaa61cf363c6"
  },
  {
    "path": "desktop/tests/asset-drag.e2e.js",
    "gitBlob": "af587c469bfceda29f5ac0f3de2374c4fff99798",
    "sha256": "13ac55b10644c2d29847b8759cec78291b325f1c8a883e38efee3d7ebd1c0c97"
  },
  {
    "path": "desktop/tests/axis-drag-dom.test.js",
    "gitBlob": "a72a42b37a8c4baf7a170560e81992f17cc9df8a",
    "sha256": "e686d2de1feebee90c6e86d73e8ad67645d96c01bca436858c5b4b70021ce048"
  },
  {
    "path": "desktop/tests/axis-drag.e2e.js",
    "gitBlob": "4aaeb425623cd3cd103ea74f81abac7bb2f81ca2",
    "sha256": "f75ef886dbfe1d2ae911481310b079c853e4b71df4845e595bde7d78ffbb8209"
  },
  {
    "path": "desktop/tests/blank-artwork-dom.test.js",
    "gitBlob": "dab21379b72f1c32318a464d444db27e05f4fbe3",
    "sha256": "90afdd87dc53b730bbbca072b7cc8c6c0f3725c1d29ebf8e8f1a79eb5d402f16"
  },
  {
    "path": "desktop/tests/boundary-controls.test.js",
    "gitBlob": "e002455943b8c354f5af27161d4f5c6bf9040cbf",
    "sha256": "930f92fdfbfecf53bdd67ffd46ac651f4021f91a10185d08d97d14eeef1120aa"
  },
  {
    "path": "desktop/tests/browser-fixture-setup.test.js",
    "gitBlob": "0ae596538c038b8df6b76aa0c7b57c57616056ee",
    "sha256": "1e99e94834eab5d4bf42de9fc52ceb26c427bfbba61d3111f996680e53f0acc8"
  },
  {
    "path": "desktop/tests/canvas-artwork-marquee-dom.test.js",
    "gitBlob": "3aa1a1d60ae8ce5d77e51a6901204ac9a97bd7cc",
    "sha256": "fc08332702bfcd6bd883f4a368e96848841f3733bae627bd169022bfe75485f9"
  },
  {
    "path": "desktop/tests/canvas-artwork-marquee.e2e.js",
    "gitBlob": "8bd12cd937456dfe62d8f3bfd686d65b0708e714",
    "sha256": "c334db52b6e404414d3c022fb7e7016335e7762016190e9a74b3664b9f055800"
  },
  {
    "path": "desktop/tests/canvas-in-place.e2e.js",
    "gitBlob": "360821ae2780d0646fdc37f52da647aace1b8cd9",
    "sha256": "2c884a992e91b4e81445c6c12a0abd8ecb0fe5ba63fbecbcb9ae5ad82d842515"
  },
  {
    "path": "desktop/tests/canvas-transient-panels.e2e.js",
    "gitBlob": "bcee0edb47830cb522f6739c32ac9d659ae963cf",
    "sha256": "8b3b70042a258bb3699a3ce5ac7d25559de2b37c07034e12b9ab3d5e117b3018"
  },
  {
    "path": "desktop/tests/character-numeric-scrub-dom.test.js",
    "gitBlob": "5f4b1aed776ba408449deed1cedd4648530432bb",
    "sha256": "85cea54db4de904191500a9928103d0c7d8a79535d91a4bdc21d82b03181a2f0"
  },
  {
    "path": "desktop/tests/character-numeric-scrub.e2e.js",
    "gitBlob": "df5e8a7170e54f45c532d6feb1765868490687f6",
    "sha256": "d8557d3a50f2c47cb538ca71163c67b96f6a60ed87ed0aa18b441f65f568cb86"
  },
  {
    "path": "desktop/tests/character-position-dom.test.js",
    "gitBlob": "33c461370141fe8ddfefd38282332dd79631f8e5",
    "sha256": "90d30c9ed1da2f5bf6ec49afc24dfb667a94167447434d6d39a975a1786c5041"
  },
  {
    "path": "desktop/tests/character-position.e2e.js",
    "gitBlob": "83417708b1048f1038e226d987246f7d3203a6fd",
    "sha256": "426a800377f664f89792ec941d547f6926fbcd49a11221ba11ee44f54c1b17ca"
  },
  {
    "path": "desktop/tests/character-snapping-dom.test.js",
    "gitBlob": "ce8a8dfb7c463f0fbcae0f5590787983bfb6be6a",
    "sha256": "dc64f275d863a15ce88d8ab7e4f7744dde051912086a535281851faef4716cb3"
  },
  {
    "path": "desktop/tests/character-transform-dom.test.js",
    "gitBlob": "22b3d8505baf6bdde0f148ac122926dc01b68dd3",
    "sha256": "990b81fed9888d3d20f2c69a857e0a634bd7b70f8f7e2fbda5178739fd8304ff"
  },
  {
    "path": "desktop/tests/character-transform.e2e.js",
    "gitBlob": "ba01f4141ca13568d45a2fd04931ddbdcb763adb",
    "sha256": "0360260b24a6ab100ebb7dd18fad97ce2a43793380ccd089afa7e7766a9d3113"
  },
  {
    "path": "desktop/tests/closed-offset-dom.test.js",
    "gitBlob": "b70c7650930bef0d3aa33bb16d09809350c092de",
    "sha256": "f7827eaa4affe36c512f59303548c0c16dc50bad27a09613e3d055e46421d797"
  },
  {
    "path": "desktop/tests/closed-offset.e2e.js",
    "gitBlob": "0b8ebc6d439900d2b7ba39b3564dcc462a4d9da3",
    "sha256": "7d1cf175a82482ca6ceb0d1c07e9bfe91b0cb76ff3becee0fa89f0ef343f6986"
  },
  {
    "path": "desktop/tests/color-operators.e2e.js",
    "gitBlob": "44e6b3706d044a098f55f6d40e378bd0b95cb5ed",
    "sha256": "70ba6025db17878f9e925c1daf6e013337a250edec48212715cbc908387e5274"
  },
  {
    "path": "desktop/tests/color-value.e2e.js",
    "gitBlob": "ed8b9f7ea92f099c6a6e8cbe51bd172789dddc4b",
    "sha256": "e159a027ad878f110f8537fb7ff9d3ff54836d9c03f7eb1b4258ee9d42df2147"
  },
  {
    "path": "desktop/tests/color.e2e.js",
    "gitBlob": "d79f45e3fccd0afadbfc51ed8494842a59eb925e",
    "sha256": "247296b56a19a4e5aca4558e67056c7b7d9b111444f6ddb8e48ec8c3c18d42d4"
  },
  {
    "path": "desktop/tests/command-download.test.js",
    "gitBlob": "63ac63bd5c1ee857239b1d9eb2a42fc47bb69674",
    "sha256": "4fe53f999e9c7fd3974a4d25afb4275a0b51770625753fa4fb2eb34f0963201b"
  },
  {
    "path": "desktop/tests/command-menu-actions.js",
    "gitBlob": "f36c5bd04be7529dc20fbc2dbc73b7cd13241d70",
    "sha256": "790bd6b80d544bb3a54065d77ea2886370a864b2776f94bd3be77c201a4bfcf8"
  },
  {
    "path": "desktop/tests/command-menus.test.js",
    "gitBlob": "1c16b9f19d9344e165dea713c194f19af4439474",
    "sha256": "8d0a4033689820eafa25869fc77691a1fadca76dcab4b95d9990621517b158f5"
  },
  {
    "path": "desktop/tests/command-session.e2e.js",
    "gitBlob": "0180fdd7c8afce10ec84dbefb4809f685aa66669",
    "sha256": "11f4a2672eab114b3d1dadac2ffba6d8c00d7772e10e0553e90d5c4f70af8aaa"
  },
  {
    "path": "desktop/tests/composition-reference.e2e.js",
    "gitBlob": "98e2a3b9837360484db98cc714f2d5a77599682c",
    "sha256": "e387de5ffc0cfaa0fc9b3d1b5ef9e4dd68bbf8e4f1980f5fd95ee3e10ba1953b"
  },
  {
    "path": "desktop/tests/creation-dom.test.js",
    "gitBlob": "4e0d65266eddb7273c080543243ead3e1dedec8a",
    "sha256": "bc3a41fbf87ac34ac825aff35feff4e691b6a81723154a45acb5334371082b53"
  },
  {
    "path": "desktop/tests/creation.e2e.js",
    "gitBlob": "861cf0d59e18ddd465e6b657f2602ce56a34f523",
    "sha256": "44605292b3dbb3ae1e41e598215e0e32b43687e710bbf88f91aac966defba5a6"
  },
  {
    "path": "desktop/tests/curve-artwork-limits-dom.test.js",
    "gitBlob": "56c3a8d02b7c3975227cc6ca0798ec059b183a46",
    "sha256": "47bf4c3fdfc649a0abf9ae3ab0215a55c40235a9b5e4dc8da03ee4966e22e9a5"
  },
  {
    "path": "desktop/tests/desktop-font-text.e2e.js",
    "gitBlob": "e1dc7a7e86a6c4e4d934538123b45d222c640d5e",
    "sha256": "bebbc7e92cbf0f57d85d0392b65fcc88e854ae2f03fc809e193a12ad773b9776"
  },
  {
    "path": "desktop/tests/direct-anchor-marquee-dom.test.js",
    "gitBlob": "dd5c0ddd27c36488fa8c3defc6dbf2a4f082775e",
    "sha256": "cd9f147fe34ffb5dc5e5542d576097615a53a09a3016c36e1853ab88e9dc2f9a"
  },
  {
    "path": "desktop/tests/direct-anchor-marquee.e2e.js",
    "gitBlob": "7b8b0e3069632fe33ebb5e7cba36c8fd130071be",
    "sha256": "4f7dcaaa9e1c51017d93042cc26a9f22f9d0d9efa0dab2c034580b4d68e488bc"
  },
  {
    "path": "desktop/tests/direct-selection.e2e.js",
    "gitBlob": "3fe5688724d886513ce10a213663d9266b605ca7",
    "sha256": "64652011c5c19373b0b752940f862ee00853323c1094043c28e87522209545f4"
  },
  {
    "path": "desktop/tests/dither-controls.e2e.js",
    "gitBlob": "5dfff1da21b8fc4e6a9b090969cb6c7880856315",
    "sha256": "af3f580a3e8a2a8f62bfa98e8f8935c411e924d4cdb90a51806e5fb3089b91c7"
  },
  {
    "path": "desktop/tests/document-bindings.e2e.js",
    "gitBlob": "ec2e0366973a5b07d30f5571b2e531f13b36d6e6",
    "sha256": "60d8b938d0ae9b5945efe5865f15635a0fa609b8b951653e28e65203b8cec2bb"
  },
  {
    "path": "desktop/tests/document-lifecycle.e2e.js",
    "gitBlob": "4572b9392d417f33d3660c1200ec918b0ba96d13",
    "sha256": "020cf2e63076a8c6122b016cdfedfe773426f7dfd307b039a57b3e5d9be1b9c0"
  },
  {
    "path": "desktop/tests/dom.test.js",
    "gitBlob": "5de50ec946b8a117ba04ca3b27634594a0d3b742",
    "sha256": "b7346d6007fdf49fb1a9d5fd1f2ff141addc208dddecee7df7a539062186f012"
  },
  {
    "path": "desktop/tests/editor-names-bypass-dom.test.js",
    "gitBlob": "217bede27f02a54d495abac2b9c5782d4811c4a6",
    "sha256": "dc68633f01826411cfb19cdc72af1f60029ffe01d13ddbe8e3c082bd998e48e4"
  },
  {
    "path": "desktop/tests/editor-names-bypass.e2e.js",
    "gitBlob": "159ca09733dabec6bf9601526e075c3627675ded",
    "sha256": "7754b9a9e4250581f2700e55dc6a8b877b07a8be2eb080d5d5172ac37b76caa5"
  },
  {
    "path": "desktop/tests/error-diffusion-dom.test.js",
    "gitBlob": "6d7b5751b00070c18f2aac9c09ebee1121e4ba16",
    "sha256": "d6c27b9dd4f85c8d5887fbb476c9117c80755f251ecb38704bf70e84f1ee65d3"
  },
  {
    "path": "desktop/tests/font-availability.e2e.js",
    "gitBlob": "913d31166e62a3fd1ef4482226e24481e9239edc",
    "sha256": "2e1fcab08d837a872eb4de280e6d8aaedf64e695fb9d34f58db6793cc1c2adc0"
  },
  {
    "path": "desktop/tests/font-dom.test.js",
    "gitBlob": "4e6b81931216f883de1e80ac6686db8888f4da95",
    "sha256": "b9700cf71796e608ffcb1aee6644e04e3b46eb1bf640b96e9950ec27870b1806"
  },
  {
    "path": "desktop/tests/font-switch.e2e.js",
    "gitBlob": "2e786bc1f4b9195c6d5e15af5b987b372725863d",
    "sha256": "1038636f82c673c57dffffb8fdf214202641ac2bfa7278e65bfb0a4e6ad1167b"
  },
  {
    "path": "desktop/tests/graph-interaction-feedback.e2e.js",
    "gitBlob": "0fdb71e34ce1f704728c6054506deb79b6b08350",
    "sha256": "8af97f5646805e8a664fa03126a42a75d0245a5fd4b31aa59523918e700287a5"
  },
  {
    "path": "desktop/tests/graph-navigation.e2e.js",
    "gitBlob": "273c926f65237d5dc43129bc33ec382ecc98e0d4",
    "sha256": "972538ba09582214ccaf589994b8e61d0110f34abf3ec776c5323dee3af63b4f"
  },
  {
    "path": "desktop/tests/graph-overview-dom.test.js",
    "gitBlob": "9407c700c184e7e3534c4cc4618187bbbc47095e",
    "sha256": "e3b131628c627185aedc9050c4994b428f6d72e4fa310aac33638174128d3fff"
  },
  {
    "path": "desktop/tests/graph-pan-joystick-app-dom.test.js",
    "gitBlob": "5e0d067e1d398ce20b4965d07289055852af02d4",
    "sha256": "7cbba22fcfabe45e6ebf9a3dbe0622f7f617216d0fbabaa80678725a704a0f20"
  },
  {
    "path": "desktop/tests/graph-pan-joystick-setup.test.js",
    "gitBlob": "8c3761ab1d370d0e8cea61657edff601b7a00fa6",
    "sha256": "38a6dd10b0a10d96640afef03752e57f51d08a3ea28d58a668f19fc3712b61fe"
  },
  {
    "path": "desktop/tests/graph-pan-joystick.e2e.js",
    "gitBlob": "1b9944269b0ef4fabaa04840b454445b573ee5b6",
    "sha256": "73c3890eed1d01a1a96bfb27f426ac82319a578a7f0ff43fb28bad802b5dd681"
  },
  {
    "path": "desktop/tests/grapheme-app.e2e.js",
    "gitBlob": "538724d7553fa06f6c12196f73e2d550267e03b6",
    "sha256": "c39d8f7e525d9673b7c9910930cc4b01f54abc93e61dfce01593d0d5f6ee411c"
  },
  {
    "path": "desktop/tests/grid-origin-dom.test.js",
    "gitBlob": "a0ad8de8c174d555c0998142cd60f6e5e67674d8",
    "sha256": "7c9952362bd6001d51ffd2f01e65e386b84b5290ed578a9b5597e72d96fc5ab9"
  },
  {
    "path": "desktop/tests/grid-origin.e2e.js",
    "gitBlob": "caa4008cf74911f3c7d116c7239d3602fc243cc0",
    "sha256": "d39ec4d201f217be573a656dca85390e613a453a2974ae2be2804f5c4a6d4af8"
  },
  {
    "path": "desktop/tests/guide-focused-escape.e2e.js",
    "gitBlob": "2f244e783a6a04add2ad5497676590e92c5a5422",
    "sha256": "bea00ccd8082d988bd34d768182404276e3a8a9c6f5d3c3625336c66a44182c4"
  },
  {
    "path": "desktop/tests/guides-dom.test.js",
    "gitBlob": "c4a60b316541a0f7628adc50a0d0945d5a9a2662",
    "sha256": "0a8b0b49e505f709e1106f9e3105cf837f806335879ca1072fb0d54f59291e00"
  },
  {
    "path": "desktop/tests/guides.e2e.js",
    "gitBlob": "33003b0fa64cbefa04d43401ecefe2738f456dc3",
    "sha256": "c4e7ce60ff49c0aee673e4c142fa721f482134ef6f9962e6f8fe2f19a0cc65ac"
  },
  {
    "path": "desktop/tests/handle-polar.e2e.js",
    "gitBlob": "548bacfbfa2c3d409c5815276fa66bcaddf9c6f1",
    "sha256": "1181c2c1b4c09af579b7edc074ff54a6daf0fbc9ea8b0d6f45e1a0a597f0f5cc"
  },
  {
    "path": "desktop/tests/helpers/artwork-panels.js",
    "gitBlob": "5a1ed5c571dd9fd1ae34086cb059f18a89dbd5fd",
    "sha256": "161aba20bb7e5ab4603fb8d28b90ff2011f5e1f5a11c369e0768bf57c4caa1bf"
  },
  {
    "path": "desktop/tests/helpers/boundary-controls.js",
    "gitBlob": "ebd8724447c15759d6203c5ad87836d8396b4c3d",
    "sha256": "a4878e82072d7a9acde6704f6ed57ec05094bbd11bb6bfa61ace489c5f656314"
  },
  {
    "path": "desktop/tests/helpers/dom-workspace-controls.js",
    "gitBlob": "ea551457780f00919baff3750de2206c788eb869",
    "sha256": "9039e22ef82cf431517316dd14a20832bce302cd50d8f845e2bc0adacf4d25eb"
  },
  {
    "path": "desktop/tests/image-effects.e2e.js",
    "gitBlob": "9cd0fd5c830c92708200e46c14d77fdadac51aac",
    "sha256": "7ef0f8b4a42d79f142a4ad9328bc6ac6c4c13a11c26f23e407e9c7242a0352d7"
  },
  {
    "path": "desktop/tests/images.e2e.js",
    "gitBlob": "0623a648fe983e8e7af339e98f54562908348a39",
    "sha256": "e033c7deaf812576fd119c529617a44a59ec40a2220703e29a4a892bd8316378"
  },
  {
    "path": "desktop/tests/keyer-input-pick.e2e.js",
    "gitBlob": "b2842f75214e31dcf105681828efeb8bda13ff1e",
    "sha256": "a330519a43dbafb91e9466873c4a15636ceb3056d1a618cbbfb4416f09cce61d"
  },
  {
    "path": "desktop/tests/layer-reorder-dom.test.js",
    "gitBlob": "b64f83bb03b5fb31a2c51da51a6775979333e053",
    "sha256": "bbe207e7c05c27b2f21ab85a24672bd51e8aef9e19608efc940b0631e7ee6073"
  },
  {
    "path": "desktop/tests/layout-guides.e2e.js",
    "gitBlob": "bc419450a3a17015b71be88816aae9b108209d2d",
    "sha256": "5b325f4f9ffb4e9ecebec7ec45f5041cdc2c9749ba3113703e8bd3ad8f5c49f5"
  },
  {
    "path": "desktop/tests/linear-gradient.e2e.js",
    "gitBlob": "c8032e9c975730d1d564680ec0b2f7e2ee6b6952",
    "sha256": "6489b09caebfd97b2a1640c75d7fe7839de1fae4ee28ebaef97baa62a8437540"
  },
  {
    "path": "desktop/tests/luminance-points.e2e.js",
    "gitBlob": "ddb1c2bf6b65b8bb5a278a47b97d803fe345d911",
    "sha256": "b541540f8f3b4041955f6282acd0dc3f83bf7fc716db22bd9672757fa6ad7a61"
  },
  {
    "path": "desktop/tests/maintained-spacing-dom.test.js",
    "gitBlob": "ee5e38fe919963994757f97fba77a95e690d50ac",
    "sha256": "1de8debf4a3f5d83711d48f3ca85e9a8f96708937278f5df9c738d4267d009a3"
  },
  {
    "path": "desktop/tests/math.e2e.js",
    "gitBlob": "81fee79b18eba2940237cd82f011b671d1bf3357",
    "sha256": "fa50c40a4aac7a57d469cc48a5cdfe02a79f83ef267a7c3ff57ddc51741a4790"
  },
  {
    "path": "desktop/tests/menu-mutation-observer.test.js",
    "gitBlob": "82b12c7458fc54ea1d81dc976fdcb6297b5b0fb6",
    "sha256": "9d3fc91a26463cdb346b1dbbf8f1cb5789b0b422c414df6f91b47ac13a95e6e1"
  },
  {
    "path": "desktop/tests/named-presets.e2e.js",
    "gitBlob": "c53318a471dddf076d98dbf7671670c26e374dc5",
    "sha256": "806036af0dcc89e059cda3e3ee660d03f5f0dacdfd6d37a138aaaae024b1873d"
  },
  {
    "path": "desktop/tests/node-clipboard.e2e.js",
    "gitBlob": "68f8ac19c34b2cbdd92cb1a9575fa0a8a14d88d0",
    "sha256": "40959a805f4add0375d4b097ed623b3002d0f5b3992c9205da99e4bb75af7eac"
  },
  {
    "path": "desktop/tests/node-context-menu-dom.test.js",
    "gitBlob": "ac520b27ae407949d1c8420c44ed7b8a6ae9d27b",
    "sha256": "b4aa17a11f69fb73c3c587a39e7d46dae17391723eb2ccd30f0e2374a42f4aab"
  },
  {
    "path": "desktop/tests/node-groups-dom.test.js",
    "gitBlob": "847bf9226dbe2a45e821699aea0365e3e03a1281",
    "sha256": "e0e2f6bcdf06a49676820627a14660c5a1f24726a7da608d8511b8e7bc914ec2"
  },
  {
    "path": "desktop/tests/node-groups.e2e.js",
    "gitBlob": "4f1d821b1975c500b20227255ec12da91bf1df63",
    "sha256": "ad02b31a632af5ef418720ed9599b91c678b868947efbdf1ff2ad041f919d150"
  },
  {
    "path": "desktop/tests/node-parameter-feedback-dom.test.js",
    "gitBlob": "5becaf5bf2fe061275bbc1385976d0795d4aad57",
    "sha256": "38af4731245cb24187e0568bef7adf6e6ecab15348b685db3048bd21201ccdae"
  },
  {
    "path": "desktop/tests/null-position.e2e.js",
    "gitBlob": "e45f731f1b3b7a5243e5c119f2d9d3ded664ef11",
    "sha256": "04ef40d509b07a088904a5361b7d75f87be789b376d6e1dc60562978947d6015"
  },
  {
    "path": "desktop/tests/numeric-escape.e2e.js",
    "gitBlob": "239a56072536c372631450b0b09f5ee059d7dd37",
    "sha256": "12c10201c49cfd4d5ff7632723e2eb10bd58b94d25b784464385391bb9f1c1ea"
  },
  {
    "path": "desktop/tests/numeric-render-race.e2e.js",
    "gitBlob": "72a3a818d0cff7d787b80cfba672d50d4d1db468",
    "sha256": "0c592c3f6f16214c775b127c4d950339a550ca39d6335af0478a6921db68fa71"
  },
  {
    "path": "desktop/tests/numeric-scrub.e2e.js",
    "gitBlob": "2318afd6d9a78bcda806ac92c33c46ce4ea7437a",
    "sha256": "030cfc782645c1c6d8410d9278069e5473b3dbcb27bb415c91fc059285e6ffe9"
  },
  {
    "path": "desktop/tests/object-resize.e2e.js",
    "gitBlob": "2e2301d90c9e7edcf82eee1d6ecaf528c7a2b667",
    "sha256": "af74cff381d2c53108b3ee45be6b589a19db4d5a22ef17178e259adcad9c4d58"
  },
  {
    "path": "desktop/tests/os-font-outline-dom.test.js",
    "gitBlob": "16c2f420e4bf641e01e25a61535cf234262068c9",
    "sha256": "315bcf122146c3c1bbe3bdd451527afaaf63ef5d1d12138a13160c5a6030d760"
  },
  {
    "path": "desktop/tests/os-font-outline.e2e.js",
    "gitBlob": "9094820cecddaa7648325fc483bec8708999b37d",
    "sha256": "c4d60ccc2cf47a651fd82ab2ca92133fd537f62e7aff0af8692eab6e5bc94278"
  },
  {
    "path": "desktop/tests/paint.e2e.js",
    "gitBlob": "af9401280230ab96690104a41f5caac806754e6d",
    "sha256": "c811ef0e6eae2d712a64db5b521b1952a1334563e3ff3917b3fe7d8c53168a0b"
  },
  {
    "path": "desktop/tests/pair-kerning-dom.test.js",
    "gitBlob": "0e5a8b175dfaf80863f213b77fc87b49f1cdfbdd",
    "sha256": "b59623106a07e4373db85e3af8ea83b98fbdccf665d3bba065fcfb312845806a"
  },
  {
    "path": "desktop/tests/pair-kerning.e2e.js",
    "gitBlob": "f43884d8f2ea58b00e85c14ff8413b876be591e6",
    "sha256": "748d30b48982e65589fc1ae0fa0a1fb3512c6d45677d23f1187f6964a84577f0"
  },
  {
    "path": "desktop/tests/pen-artwork.e2e.js",
    "gitBlob": "a3fa519164e06e507c6c28a7c4b221d8d04d0ce8",
    "sha256": "5768e5e6df45cbc4bf0a8d0a802605ce1cee9836bda484850f0fe590eaddf398"
  },
  {
    "path": "desktop/tests/pen-menu-keyboard-dom.test.js",
    "gitBlob": "22962f301f6175f77ae8dfcdac89e5e832263df7",
    "sha256": "750dd7dfbab9df43f36c341d7ae733ba790aa0e8286c6e19b1a966c5d567e72d"
  },
  {
    "path": "desktop/tests/point-alignment.e2e.js",
    "gitBlob": "7d3c298eee8dcd64f4797fba549b0ba7ac89a66b",
    "sha256": "5371b7506ea6729e95061fdbd5ec6a01f62543507d5c57c3f42e8447cc67e042"
  },
  {
    "path": "desktop/tests/point-distribution.e2e.js",
    "gitBlob": "2c65b82d30603bb3d4a32ddbac6bdfbb158febf9",
    "sha256": "f744f4e3102aec0dba6518314b6b76343fa864167793a2dc6439f69550f1e641"
  },
  {
    "path": "desktop/tests/point-spacing.e2e.js",
    "gitBlob": "86eb77a623398d6fec4d49818a4fed7d5fe9dbc0",
    "sha256": "e53d683919022e1c253651d6df4f97e077903875ea723c2a8a7ba6f1996eaebf"
  },
  {
    "path": "desktop/tests/polar-transform.e2e.js",
    "gitBlob": "ab8d121cccdc3510a6bb0eabe665aaa9ab4522e1",
    "sha256": "13c68679b07debde3f09253dc301bb2c988117c60cf0a079554007adb443fe6b"
  },
  {
    "path": "desktop/tests/position-reference.e2e.js",
    "gitBlob": "156dc8c3d874b9931ba16e5848cdccd6e20a6400",
    "sha256": "e2ecf61142eada8d9d38ca53f2df4b5b00fe970ef2ae4ac09f72a08a9035048f"
  },
  {
    "path": "desktop/tests/private-font-flow-dom.test.js",
    "gitBlob": "6090de7ba59a583beca928b8398abf0b77cd007f",
    "sha256": "921c8940d6d465128b2de3d44f63165de59e59e7da4b7aef0bf7e0c9137ff13d"
  },
  {
    "path": "desktop/tests/private-font-flow.e2e.js",
    "gitBlob": "a5db60fa5031dd8cc0990c09b38b32911e6c58fc",
    "sha256": "601ea0d8d80cbe85ef8821edc9d993c3997a0d756075f1e0e6cb0c600e69a5e2"
  },
  {
    "path": "desktop/tests/production-csp.e2e.js",
    "gitBlob": "6feaad92afa6ccefa9f4bc1a9444ebaf4a01abec",
    "sha256": "b5654273b97714933a723540fdfbce661e7f6460691a2f81af21ccefe7d38a81"
  },
  {
    "path": "desktop/tests/project-composition-import.e2e.js",
    "gitBlob": "eaededf649d547b5cdfb1d98141dde6086d0ed7e",
    "sha256": "d499c9f0a2b77ba6ca3e05c0042a7f449490fe31adefe2d24c08cd4231b69e7c"
  },
  {
    "path": "desktop/tests/project-integrity-ui.e2e.js",
    "gitBlob": "84b3d884e0c8cc040b4bef7655d93b7448888dd4",
    "sha256": "b5a161f2013a98ea36cbe86e61f55386e8be4069c1213c8d3b6a111310b30df8"
  },
  {
    "path": "desktop/tests/radial-gradient.e2e.js",
    "gitBlob": "a6ca6ccc078d2cbc12f95da0b520cb2ffab25b0a",
    "sha256": "6a129376c73fbfda0a3cd2ccd6dd26a38d02b81336a62d5b63ef3f5660d77a16"
  },
  {
    "path": "desktop/tests/recovery-failed-context.e2e.js",
    "gitBlob": "3902de92f06e12599287e6b1dcbaab806a4974a3",
    "sha256": "364311be078aac4d54b64678d3d8649fa1278034a7541f5c9500040b24477ecf"
  },
  {
    "path": "desktop/tests/recovery-per-operation.e2e.js",
    "gitBlob": "0aac9bc47eb02aa500d5241e1d1d1ff658ecef9d",
    "sha256": "533b247618998cfa076edc2d5432bc98710e7305387bd9f95550e42a4b01f9d9"
  },
  {
    "path": "desktop/tests/reference-status-help.e2e.js",
    "gitBlob": "57ef845d4262de8d0a72cf7044165ac6de3636f0",
    "sha256": "43b6748e7334297cc1efad2b36e3614e43bc70445dcdcc4ab0f67722a6c39d66"
  },
  {
    "path": "desktop/tests/repeater-ae.e2e.js",
    "gitBlob": "3512c02fab7bd8447fbe406fba6574a35645d346",
    "sha256": "129a04aa3316a8a2339223a41b41cc22eae571e13e80263a8166b8176fe2b206"
  },
  {
    "path": "desktop/tests/repeater-alternate-fill.e2e.js",
    "gitBlob": "dacef2a88fe9c3b7c89db8fcffa23b93451ff675",
    "sha256": "5ad884c21f4f73723a3ba3a27d38cde743b7746c10489a6a2eff8fb5edd887a0"
  },
  {
    "path": "desktop/tests/repeater-alternate-scale.e2e.js",
    "gitBlob": "77d6ca6a89dbfce1a4a58a5ab83c483fbccafa42",
    "sha256": "d84dc7334f13a8b7788fbb2c52014c09de2bceafb4f60707bba406f693c7b301"
  },
  {
    "path": "desktop/tests/repeater-angle-dial-dom.test.js",
    "gitBlob": "e2dae4e736ac8dc15577cc5668730172a468dc7b",
    "sha256": "120bb78b67da94a375ddd17547b0447667ae58d678af0aa462ef51e8454b9a12"
  },
  {
    "path": "desktop/tests/rich-text-dom.test.js",
    "gitBlob": "d997534ea1740b212372ef9f3a8e5d55e1ddb265",
    "sha256": "ea916c3582dd6252e3a78736e9600e23abefbfb7202fcedca0d41d3905a0016b"
  },
  {
    "path": "desktop/tests/rich-text.e2e.js",
    "gitBlob": "fc22a89b29ccd1021edbb3281076199d997b9e2e",
    "sha256": "21a0f7bffa584873104aead2296a42dc4ab9c2784da3d841797cc7d0ac5eda00"
  },
  {
    "path": "desktop/tests/shared-colors-panel.e2e.js",
    "gitBlob": "45076bb0b3afa454e2f98988ef0e705ec2e838a6",
    "sha256": "e8a08757ecbcffdaf997b52d52e575d4b5c62246149bb48d030fb1f95455b5aa"
  },
  {
    "path": "desktop/tests/source-navigation-dom.test.js",
    "gitBlob": "a6bf3b81b473cd2088340e76ef3d38d4f5d1cfee",
    "sha256": "ec8e0ffae1a4d32a61fc86f9565b1f60e19b6b399f324b9afea41c049eb3f696"
  },
  {
    "path": "desktop/tests/stroke-style-dom.test.js",
    "gitBlob": "9fe379ee4e03c7d162017dcb8c460a0d19db8fe4",
    "sha256": "4ec9f3b6a0ced15366a1c48a9c7c1dfea14f759427f7ac107f72cfb184f60756"
  },
  {
    "path": "desktop/tests/svg-part.e2e.js",
    "gitBlob": "ef3337d7ef13e0e79bd0ff6411b1c99bbfa4e59b",
    "sha256": "2a9ce0f059fdf9457d96bcff944bb081823c1ba9fe0dbc1d87ea3a301d3ba1b0"
  },
  {
    "path": "desktop/tests/system-font-dom.test.js",
    "gitBlob": "109c444c2f1af06686a4f48405fe55853b1d2286",
    "sha256": "a9da397a79cd38ea074808a0f8510899a090a1d697b30418ec67390844eb297c"
  },
  {
    "path": "desktop/tests/text-drag.e2e.js",
    "gitBlob": "42f3261e28aebe9d7dfc62b55b899ce5268ed047",
    "sha256": "b1e837edd631dcf11b6ad9edb5ab064df2c5d6ec7d91a0aeb4c04255b48e1c8c"
  },
  {
    "path": "desktop/tests/text-inspector-layout.e2e.js",
    "gitBlob": "96bb745c64468d3aa7b2318b2470c66f392f804c",
    "sha256": "421a830dc1e83f85e3da76b93dc3ca54713e70729079f6666a3733de6840e1af"
  },
  {
    "path": "desktop/tests/text-outline-path-ui-dom.test.js",
    "gitBlob": "c5d903253bd00a6b3208951a301df1e8973f19b4",
    "sha256": "ada9a893ab82d89e8d7814beeba097803b76609c3272ffa39047a824ef5e7025"
  },
  {
    "path": "desktop/tests/text-path-dom.test.js",
    "gitBlob": "0d61eaee90779cb9b7d9d21bb306e729804dda75",
    "sha256": "811c208fcda80b6dd1ecf7fcd04c6185f122fa1a92cc90456b2bdf320f394afb"
  },
  {
    "path": "desktop/tests/text-path.e2e.js",
    "gitBlob": "ae206e05449685fa2b37ea514de0a00affcdadc7",
    "sha256": "2e4462bdf52f574ae8a4c8c667ba9b1006a6d7377b361d0bc855180797cfc567"
  },
  {
    "path": "desktop/tests/text-position-output.e2e.js",
    "gitBlob": "f7b51cf0a68c76d97809c9ba37d785d1aa348375",
    "sha256": "3276dae494ca054d9278601699872bf97614c79ad3117e26b68ffb630d998f02"
  },
  {
    "path": "desktop/tests/text-tool-dom.test.js",
    "gitBlob": "69c15cb064f3e785a25062e3cddff6783d7947d2",
    "sha256": "07406ce40666ec1af5d905ca18febf597513878a09c5ed265c8871b85de6f717"
  },
  {
    "path": "desktop/tests/text-wrap-canvas.e2e.js",
    "gitBlob": "3b78afd458833a216e995c2e83639c24c6b11481",
    "sha256": "8ad093c46c8bd191b0aaafc0896143696b02e7e051cbd45570ed5f0ec167e3e5"
  },
  {
    "path": "desktop/tests/text-wrap-dom.test.js",
    "gitBlob": "0568904d01cca68bd6e979a72005c4312f5121e8",
    "sha256": "932e36eb03ec28b5a388b830536d543fcf0eb7a07b3ded138f177390bf36dfd7"
  },
  {
    "path": "desktop/tests/transform-anchor-dom.test.js",
    "gitBlob": "ad10aa944c1b06bc8be19df39470dc856a35aecb",
    "sha256": "c2cb1981daef247055b518e6e698208bfee5573a7c6ba2f045d81c56cd90f319"
  },
  {
    "path": "desktop/tests/transform-anchor.e2e.js",
    "gitBlob": "57b57999243f74dac53500102a5359a9b5f9b1a9",
    "sha256": "01bda7c8a154e11fc9a26188e49f036cb273c1b5ab62a79fa92e3b54141658e2"
  },
  {
    "path": "desktop/tests/transform-handles-dom.test.js",
    "gitBlob": "8effe8c12538513d13159fcaf5d13e6f8ed2ef9f",
    "sha256": "3039898d60997dc72bdff3c327ce49cf9ca1a291e574ad09a8dfae46e02bf58b"
  },
  {
    "path": "desktop/tests/transform-handles.e2e.js",
    "gitBlob": "bf61fda043846df52a688b6651b1a73d54d5463c",
    "sha256": "bbe1d0f6d977c57eb547049df41fe3a24eb0205509ee8a4615f9dcd19ee955f3"
  },
  {
    "path": "desktop/tests/trial-feedback.e2e.js",
    "gitBlob": "2d5af76681bc09d049e4c7bfd750e3819d7c478a",
    "sha256": "f0533a2abc3b2eb1d0870089a5f20e6107ae56477d9ef3aada8af86f421667bc"
  },
  {
    "path": "desktop/tests/ui-icons.test.js",
    "gitBlob": "94e01a6d5291c97fd0a4ac880ece89fd77623580",
    "sha256": "fd3e56ff66577131f543552da0e8f0f80a7d8010d27345afc486f97413b4dad9"
  },
  {
    "path": "desktop/tests/ui-review-captures.e2e.js",
    "gitBlob": "7fb7857eb3afaea6644fd4bdea0a111b0f326fab",
    "sha256": "51307043cd0de33002e56016538d77126e1d748360fdf6dddfd5c056cb3a1f83"
  },
  {
    "path": "desktop/tests/unified-font-picker.e2e.js",
    "gitBlob": "3a89803f0de45e6eebd7e2ec5b8125ba85c20800",
    "sha256": "a8171d913c768bed986ada39ba40912c9a3f7bde7876dfeff5ef7074500e3303"
  },
  {
    "path": "desktop/tests/variable-font-ui.test.js",
    "gitBlob": "5e360958415870ab267fff80f2d8d26fd77f90ef",
    "sha256": "98548861d702f4fd86567f038c74ac2f8584b817a3e613d70964b1f56f99ac76"
  },
  {
    "path": "desktop/tests/vector-position.e2e.js",
    "gitBlob": "f179a9e4c800713373b6186703c2f1114fb5600c",
    "sha256": "8b0f15fc21f4b211c58bd40e05db44b3595130dd1a294ab72b2c7d85f027b11b"
  },
  {
    "path": "desktop/tests/vector-reference-dom.test.js",
    "gitBlob": "516f70c37bb12eee213adb6b47d35eddfea74684",
    "sha256": "5ad1ad8ca71cb2bdeb28593639a3ba5b6374ea9aba7264cd6a9eaa0d6fc5f2ce"
  },
  {
    "path": "desktop/tests/vector-reference.e2e.js",
    "gitBlob": "133ade1677f9a96eb94f45d5e64650dc3a85218d",
    "sha256": "308cfd244daf12ef8996aca1d24aada5d43e2a27fcdbde9b9b2e2d0d66566f5b"
  },
  {
    "path": "desktop/tests/view-navigation-dom.test.js",
    "gitBlob": "9a152a67b6d2416ca2a4d1370555d23d66e415b3",
    "sha256": "c02cc64fdfe668c919ff6d038a2423a24d498f3c3a261b6163848510e24ef1bd"
  },
  {
    "path": "desktop/tests/view-navigation.e2e.js",
    "gitBlob": "ac0131288feb150484a99f0398095cfa99c01d72",
    "sha256": "78946dcdffe4ca4bd06485d98be0cc205644a650ada6e2b04f220b603a594eee"
  },
  {
    "path": "desktop/tests/workspace-panels.e2e.js",
    "gitBlob": "a366bb8c9d459d4397e416f203e3597b21720044",
    "sha256": "f4fa882f2c06fe2f0adb5406ab9025dff8716a2f522e43b407cb2d1e33650090"
  },
  {
    "path": "desktop/tests/workspace-panels.test.js",
    "gitBlob": "a462c35a6da7254b4f2d6feccd23322b20438d04",
    "sha256": "c41f27977d5b7d2fbf874a678343789541ac10a2c7ba305660aafa2ef7cf3b75"
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
