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
    "gitBlob": "855dfe4e94f258a6d16f149af63f56757a859959",
    "sha256": "0a61529da4f27c2ca05d0af094107a56aaca5693e98ef3ddaf49a2c2c080dd40"
  },
  {
    "path": "desktop/src/main.js",
    "gitBlob": "c061bcc56d50abd41f7cdff77f22f71f903022d2",
    "sha256": "01f3aa3646c546c8a8f30275a0e2bcb0b02ab93b62143f4c510d4b0fcf46ab83"
  },
  {
    "path": "desktop/src/style.css",
    "gitBlob": "45c05ac993ab276598567d48ab3f77e62b486240",
    "sha256": "3e5afeddc55b0cb2f4c4f44f7af0821cb715dc06f6902c272852a768e297632d"
  },
  {
    "path": "desktop/src/text-tool.js",
    "gitBlob": "d1771ff78a491a1c1a52676e8030000332b0f7d2",
    "sha256": "82900d2959efb5c4c9da8b0f54e189a19a6d3f2424b5315501fb768aae0e1400"
  },
  {
    "path": "desktop/src/workspace-panels.js",
    "gitBlob": "dcf5f01e1fe8afd918cb126ac3ab90171bdfb96d",
    "sha256": "0ba07efca769fc016083c6e05485abbcfe03166e74413d5be03adda7387796c2"
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
    "gitBlob": "112a9b318dd9526d9514d654a3f3b8aadb6a8f3c",
    "sha256": "647061f169b7edaf817e61ce480a6f22d0ffde1a1bef3b402dd193fc268c8e65"
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
    "path": "desktop/tests/artwork-multi-move.e2e.js",
    "gitBlob": "82a3aec1d642bc7414cf008ce6112c929b9a38a6",
    "sha256": "87e28477f94e9fd8a42402ae454c3f942395a264e0f50a3362eb02e747a44575"
  },
  {
    "path": "desktop/tests/artwork-ordering.e2e.js",
    "gitBlob": "2ff551d4301c19335be30ca5bb78090dfa148ec1",
    "sha256": "fbbdb8e94a846cc2e42611a654d29dd8736ec6bc028ece34c7b6bb9ca8358842"
  },
  {
    "path": "desktop/tests/artwork-snap.e2e.js",
    "gitBlob": "27ce3049daeab1410e482171b575c838c2e752c8",
    "sha256": "e53e5a6b182a4f32ecc70280f4db8b5f259f20dac797e37cf2be2c4b22b1c6f8"
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
    "path": "desktop/tests/axis-drag.e2e.js",
    "gitBlob": "4aaeb425623cd3cd103ea74f81abac7bb2f81ca2",
    "sha256": "f75ef886dbfe1d2ae911481310b079c853e4b71df4845e595bde7d78ffbb8209"
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
    "path": "desktop/tests/character-numeric-scrub.e2e.js",
    "gitBlob": "26894d747e8872a39dcdc8b08fff32cb0292599e",
    "sha256": "39c662eedec6038ed406f0c0f0fe8ecef4e0c1223b689d93dbc7a1d6276b9dc2"
  },
  {
    "path": "desktop/tests/character-position.e2e.js",
    "gitBlob": "5a6ef2a5b8690b2dfded55a15f568e4ea6609469",
    "sha256": "94f734f42a779a0f9f51d9ee9998e3438eb65fcd26a5a9d6851566a3de9911f1"
  },
  {
    "path": "desktop/tests/character-transform.e2e.js",
    "gitBlob": "9d496d3965e03fa9ddb5dba07cac15317ebfa61f",
    "sha256": "d6579afaa36a547e3387046288b4a2217a8b02696f9e744ca37144e06cdb15a8"
  },
  {
    "path": "desktop/tests/closed-offset.e2e.js",
    "gitBlob": "615246ce2a05492ade336daf2686e888368997b6",
    "sha256": "dbfc101f914d7f5f075a43e13a95df0c2cd9b2901ac0939d2e82ae0fa75325d9"
  },
  {
    "path": "desktop/tests/color-operators.e2e.js",
    "gitBlob": "6ddd964fccde300a3a51846ade2f5a20ea6d1e8a",
    "sha256": "9de2fa3aaa47ec5d81389277ce5810a9beb1eb8fb4a202d6acc1a2f6e0c09757"
  },
  {
    "path": "desktop/tests/color-value.e2e.js",
    "gitBlob": "71539b45051fe8e11642cf966bb4978b2a34f3d1",
    "sha256": "af8d8d6723d8740728f26fb847b2ab02e986b7b2416a2c7252bbd44033afc8a6"
  },
  {
    "path": "desktop/tests/color.e2e.js",
    "gitBlob": "5a9d49effb369072e7587da17789e4874d42bdf5",
    "sha256": "c45745015a1642cb63d8ea92aaa28527a5f60087d33913ec755fba7f4f659c94"
  },
  {
    "path": "desktop/tests/command-download.test.js",
    "gitBlob": "63ac63bd5c1ee857239b1d9eb2a42fc47bb69674",
    "sha256": "4fe53f999e9c7fd3974a4d25afb4275a0b51770625753fa4fb2eb34f0963201b"
  },
  {
    "path": "desktop/tests/command-menu-actions.js",
    "gitBlob": "0b112c689c92d4f5b59da71f67c204efd478d3fa",
    "sha256": "9224de03ddf7c72cea2ad0aa289a95f69156de3cfabd8704305db83fbcac66de"
  },
  {
    "path": "desktop/tests/command-menus.test.js",
    "gitBlob": "07c1f5388e0eadd16428078ffcf451d17cb3872c",
    "sha256": "c5f663230241a27d0fba481fbf40d801cac44cc166b67744064ecfd415625261"
  },
  {
    "path": "desktop/tests/command-session.e2e.js",
    "gitBlob": "0180fdd7c8afce10ec84dbefb4809f685aa66669",
    "sha256": "11f4a2672eab114b3d1dadac2ffba6d8c00d7772e10e0553e90d5c4f70af8aaa"
  },
  {
    "path": "desktop/tests/composition-reference.e2e.js",
    "gitBlob": "acad5ed789c45c76f3cc5d23ce5f875ca701f335",
    "sha256": "4797c45519ae900f4b3b5319f82aeeb22a561a90ea50ad4ec7fdec38b466daee"
  },
  {
    "path": "desktop/tests/creation.e2e.js",
    "gitBlob": "154918ca1c75479ec123d14be85f89488922de82",
    "sha256": "6cfc6c9b0ec66f84af590fd6d285392355ea48281466ccff8d8008dc1c4e147d"
  },
  {
    "path": "desktop/tests/desktop-font-text.e2e.js",
    "gitBlob": "2eae9f766dcceb4632baec664ae4438731820590",
    "sha256": "3073c9ac74ded5d564719cd6746d6937a4fc6ea7cb5ffc79c3f06f7aa4fc538f"
  },
  {
    "path": "desktop/tests/direct-anchor-marquee.e2e.js",
    "gitBlob": "7b8b0e3069632fe33ebb5e7cba36c8fd130071be",
    "sha256": "4f7dcaaa9e1c51017d93042cc26a9f22f9d0d9efa0dab2c034580b4d68e488bc"
  },
  {
    "path": "desktop/tests/direct-selection.e2e.js",
    "gitBlob": "2427770810515df1b7ae91f389e1df4ae6f656d0",
    "sha256": "303eb52c9872ac5fecaf192cb69040359e9eee21a1cae3594ae38e6dc223fc36"
  },
  {
    "path": "desktop/tests/dither-controls.e2e.js",
    "gitBlob": "238f0bec698797d3c7cf497c8af5822a964a0403",
    "sha256": "f64c71e5e17b27fad5b59b98258a66390d815ff7bba965b1967f041cbd8f5f10"
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
    "path": "desktop/tests/font-availability.e2e.js",
    "gitBlob": "ccd64732195ee3aa9032df473bbd41139009c9d4",
    "sha256": "25adb37e683e5707555804ea449892a52df750eaf8a718130c2d44a541290225"
  },
  {
    "path": "desktop/tests/font-switch.e2e.js",
    "gitBlob": "0335a605e3446227c432973f4c90ba1657611487",
    "sha256": "772d456592ec701bcd5bff7f4504b79ba6a79892e27c471982e96490451ce18f"
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
    "path": "desktop/tests/guides.e2e.js",
    "gitBlob": "33003b0fa64cbefa04d43401ecefe2738f456dc3",
    "sha256": "c4e7ce60ff49c0aee673e4c142fa721f482134ef6f9962e6f8fe2f19a0cc65ac"
  },
  {
    "path": "desktop/tests/handle-polar.e2e.js",
    "gitBlob": "b5e468380c9d2e386feac7b8fd6db02101dc8cda",
    "sha256": "eb6c1b2d4eb7a6917835f0b1b5560511728b0736c9da32257964433292b8cae6"
  },
  {
    "path": "desktop/tests/helpers/artwork-panels.js",
    "gitBlob": "5a1ed5c571dd9fd1ae34086cb059f18a89dbd5fd",
    "sha256": "161aba20bb7e5ab4603fb8d28b90ff2011f5e1f5a11c369e0768bf57c4caa1bf"
  },
  {
    "path": "desktop/tests/image-effects.e2e.js",
    "gitBlob": "ac15f0215d9c4d6bf2af204976f80a70331388c2",
    "sha256": "b58dc74b88b3fba325a3deb4d47bcaac28e3d99528aea583697eb4077a60217b"
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
    "path": "desktop/tests/layout-guides.e2e.js",
    "gitBlob": "bbb38e8b239ba520df28114aacf80ae70aa24119",
    "sha256": "a7a0bed74089c9794607994fb34f1e3242d6fbc7be3d619160068bf7ee7b70e7"
  },
  {
    "path": "desktop/tests/linear-gradient.e2e.js",
    "gitBlob": "e1af2c5d7d619848e65cfc769c80e2e4d81dcf60",
    "sha256": "02acd1b5fd39f6480b3200d3bd37fb80fecb202dc692d244b007bb53fe17f78a"
  },
  {
    "path": "desktop/tests/luminance-points.e2e.js",
    "gitBlob": "c564e622f35bee4e8662f4a65b73bebb6e830004",
    "sha256": "1b9e4e59ecbc2a1f08d8ed2b12392824555380bc2a24e59466c149f8b36eb9d0"
  },
  {
    "path": "desktop/tests/math.e2e.js",
    "gitBlob": "f468b460502c8c4a0728f2849ac154964389976a",
    "sha256": "060dd57c78172f9c92cacbad6bc08ee5635181477ef2ec5873ed86fb28ef1ace"
  },
  {
    "path": "desktop/tests/named-presets.e2e.js",
    "gitBlob": "3bfc8aed490e1b4d7c5cd8d25b3214657b595006",
    "sha256": "33f16df9c7d2f971775d8a0d82c331816805af66d637a0d4eef78ca1bcc69879"
  },
  {
    "path": "desktop/tests/node-clipboard.e2e.js",
    "gitBlob": "5606b3acec00a59c14408b73cd81150fc3886092",
    "sha256": "a4b1bb3a10576716868c97252c847589ab908d9b2c7723bcab21d3f9a8a05df1"
  },
  {
    "path": "desktop/tests/node-groups.e2e.js",
    "gitBlob": "4f1d821b1975c500b20227255ec12da91bf1df63",
    "sha256": "ad02b31a632af5ef418720ed9599b91c678b868947efbdf1ff2ad041f919d150"
  },
  {
    "path": "desktop/tests/null-position.e2e.js",
    "gitBlob": "078f37c3fcfe232f4c8e18883e5913b1708fd81c",
    "sha256": "69cb3cee70d906fbe2f5e4b661a62c7ae31e3e6fd75e06abd7342efb0bde9e92"
  },
  {
    "path": "desktop/tests/numeric-escape.e2e.js",
    "gitBlob": "239a56072536c372631450b0b09f5ee059d7dd37",
    "sha256": "12c10201c49cfd4d5ff7632723e2eb10bd58b94d25b784464385391bb9f1c1ea"
  },
  {
    "path": "desktop/tests/numeric-render-race.e2e.js",
    "gitBlob": "67b75346be9a978daa9550329039039c39cb7e79",
    "sha256": "5401b7cde44763d24ce500f8c7fdc9089a12b7f152ec578086eaed6605f6e997"
  },
  {
    "path": "desktop/tests/numeric-scrub.e2e.js",
    "gitBlob": "afe1de4af207f0c948b69a098f1b2658d6445e47",
    "sha256": "091d0d8c2ab9b4ac8f6e1ffd4a08d2ddbcf4491793f6b52079cabea14fe69f11"
  },
  {
    "path": "desktop/tests/object-resize.e2e.js",
    "gitBlob": "a001cf92ca6df103b63f0f5440a941cd53106542",
    "sha256": "2179267b0eb76ec007ba18e2930ee8938f8f913bd3b1c8d8f50103fbf54a9838"
  },
  {
    "path": "desktop/tests/os-font-outline.e2e.js",
    "gitBlob": "91deba2ea5099c66825429c1e81ef0837bceb847",
    "sha256": "4c4e3795341b6b94ed4e13a2e1bd5164f7aff841c907bc547710c94f92815e16"
  },
  {
    "path": "desktop/tests/paint.e2e.js",
    "gitBlob": "ae733ebd7d24f8b6e14508357815dc2fed73b196",
    "sha256": "29a6dd42e84ccc98d234ac31ee4122ccc81b44e1d570cd7e11282edad5f77262"
  },
  {
    "path": "desktop/tests/pair-kerning.e2e.js",
    "gitBlob": "c3f94c0bf2de826db928873ded321a84fd058f16",
    "sha256": "b63eae12cddfc59dae503e5e3ebb265420e3c90fb0604d12bd5230a1924f81c8"
  },
  {
    "path": "desktop/tests/pen-artwork.e2e.js",
    "gitBlob": "a3fa519164e06e507c6c28a7c4b221d8d04d0ce8",
    "sha256": "5768e5e6df45cbc4bf0a8d0a802605ce1cee9836bda484850f0fe590eaddf398"
  },
  {
    "path": "desktop/tests/pen-menu-keyboard-dom.test.js",
    "gitBlob": "8efb6afafeeb51bcdd4ba5b3b9dd1da333512dfa",
    "sha256": "847a6bb998b54e6b8967736bc8bfecef736f236bb6afb8e7fd2dab2dd0138aad"
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
    "gitBlob": "082aafb3373bb18c11b8df141f6a1069adb470fd",
    "sha256": "f04d1ac3dfe0d486f424d6d07c65aea7126c2fa8488744508953e5d2db0ecd51"
  },
  {
    "path": "desktop/tests/position-reference.e2e.js",
    "gitBlob": "a9779df517d90adb6834030c68cf76c8ba2b86d3",
    "sha256": "df3fd810c275f5109ec47b1bfe5ead67e1ccad6be6e0c912c0df81b6897ece76"
  },
  {
    "path": "desktop/tests/private-font-flow.e2e.js",
    "gitBlob": "633f13072cf1c7d90823b899ec0da726d0d1d6e2",
    "sha256": "74c42c44d4bcd5326de7a0936c8d76c681ba0112516d683160b5ea068c5ad2f0"
  },
  {
    "path": "desktop/tests/project-integrity-ui.e2e.js",
    "gitBlob": "84b3d884e0c8cc040b4bef7655d93b7448888dd4",
    "sha256": "b5a161f2013a98ea36cbe86e61f55386e8be4069c1213c8d3b6a111310b30df8"
  },
  {
    "path": "desktop/tests/radial-gradient.e2e.js",
    "gitBlob": "79432e8f8d0c0bee7d1e1698e57bb3c084803204",
    "sha256": "a297a26f66d27349b025c684b807abfbfc1e8bbfd17bc1da19be12f8347628ef"
  },
  {
    "path": "desktop/tests/recovery-failed-context.e2e.js",
    "gitBlob": "5a918b845364c5e3dc644d184f607e14f677f474",
    "sha256": "b29bdaaace430f01a78eea0761c0ecefe32deddf785dec5d9008e8beef87737b"
  },
  {
    "path": "desktop/tests/recovery-per-operation.e2e.js",
    "gitBlob": "b4fcba9fa093bc046acad12ea482297e6074a542",
    "sha256": "6deaa1e3a119457489b800781802d2a639107c31410639d0703ece7108b223b8"
  },
  {
    "path": "desktop/tests/reference-status-help.e2e.js",
    "gitBlob": "8bf72dda5c9fa04fa60b7dd6295df4af16c462a4",
    "sha256": "79219837b2b660b58d41191e3db90db8eff6ac7a0647725a93ef445a39d37c8d"
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
    "gitBlob": "a12b35a916454819dcf777447b241cde3d5a5af9",
    "sha256": "4f5a1e92b2c215eb4bde8c396e5524bd619e27d7429d65072a2a949e5c416ff3"
  },
  {
    "path": "desktop/tests/rich-text.e2e.js",
    "gitBlob": "673f611a127b2dee1b2b5d0dd2f2172e386cd15e",
    "sha256": "356a049c27bbbf9917bbaf77daa5dcc22df6c5ad73206fb6e5f08be8c2220dc1"
  },
  {
    "path": "desktop/tests/shared-colors-panel.e2e.js",
    "gitBlob": "45076bb0b3afa454e2f98988ef0e705ec2e838a6",
    "sha256": "e8a08757ecbcffdaf997b52d52e575d4b5c62246149bb48d030fb1f95455b5aa"
  },
  {
    "path": "desktop/tests/svg-part.e2e.js",
    "gitBlob": "78d485a9ead10ef50607d3a19b294deb4bd35e77",
    "sha256": "3f7defb537b22803d7332e216ac3ff55e2d44c9a65793f308c31e64fde334bda"
  },
  {
    "path": "desktop/tests/text-drag.e2e.js",
    "gitBlob": "42f3261e28aebe9d7dfc62b55b899ce5268ed047",
    "sha256": "b1e837edd631dcf11b6ad9edb5ab064df2c5d6ec7d91a0aeb4c04255b48e1c8c"
  },
  {
    "path": "desktop/tests/text-inspector-layout.e2e.js",
    "gitBlob": "97d6ecdc8fcc589459df26c665d9a8229042d38d",
    "sha256": "d9de25ef62e6c3a9d72caa17985039d20927999831c0821d54834288297f1dae"
  },
  {
    "path": "desktop/tests/text-path.e2e.js",
    "gitBlob": "b98e8d39ad828a9314606e8ddcd2bbed50ea2052",
    "sha256": "185f83f0be629f42b451fcf34c602e3e147bba6ebdf5b6cfe533cbea061cf13a"
  },
  {
    "path": "desktop/tests/text-position-output.e2e.js",
    "gitBlob": "22ea6217fac43eb35054efecb5ac799f435e4a01",
    "sha256": "0a0cc8167384f8943ae4049a32fb64ecb558d9ebc02a09dea28d3960fe9f06a5"
  },
  {
    "path": "desktop/tests/text-tool-dom.test.js",
    "gitBlob": "a2215054b62c982a3f020c7afd37a88619727c79",
    "sha256": "b834eea361e630344775855c275ea276d3a2fbf6cc6fe6a253b75a0c519a819c"
  },
  {
    "path": "desktop/tests/text-wrap-canvas.e2e.js",
    "gitBlob": "3b78afd458833a216e995c2e83639c24c6b11481",
    "sha256": "8ad093c46c8bd191b0aaafc0896143696b02e7e051cbd45570ed5f0ec167e3e5"
  },
  {
    "path": "desktop/tests/transform-anchor.e2e.js",
    "gitBlob": "8eaab7c70b06548a17edb80b5f7802ce7fbd9d79",
    "sha256": "09dfef1c9b93a73c4eb07534e415c3fa091802e95e1326784be7fee12d469d99"
  },
  {
    "path": "desktop/tests/transform-handles.e2e.js",
    "gitBlob": "4eac43c9a672478bf4c755b178e4d2208f4ee4a7",
    "sha256": "56917d2d49d570b0ab552e88994c3d6d3135b21f45e621bea229518925eb08c6"
  },
  {
    "path": "desktop/tests/trial-feedback.e2e.js",
    "gitBlob": "2d5af76681bc09d049e4c7bfd750e3819d7c478a",
    "sha256": "f0533a2abc3b2eb1d0870089a5f20e6107ae56477d9ef3aada8af86f421667bc"
  },
  {
    "path": "desktop/tests/ui-review-captures.e2e.js",
    "gitBlob": "df8dd0705768cbf89f9e60228ed0092ec6f6d53a",
    "sha256": "836c031846043aa13dab038fa8d816df50968e8748b8821eebdeef6980ec4670"
  },
  {
    "path": "desktop/tests/unified-font-picker.e2e.js",
    "gitBlob": "146fcd267a94262f5ab46cd320affbb1a9d986f1",
    "sha256": "7319fc7eb5f92d8ded374936a00b53b94c8944105a107f14bb331111f8309b90"
  },
  {
    "path": "desktop/tests/vector-position.e2e.js",
    "gitBlob": "98ff07fc89ee541f2bd0a948e2991da46dce3bb0",
    "sha256": "dd1c90abb00c85f5162d2817f6760aabef4f53772f1b302d0b88da6bce8c08cc"
  },
  {
    "path": "desktop/tests/vector-reference.e2e.js",
    "gitBlob": "cdf45228b7b78f27699568ab9e687233b8d2dec5",
    "sha256": "be42187dfce36ba181d278a9338055b31940d0f7bf8634df310af573368d15e6"
  },
  {
    "path": "desktop/tests/view-navigation.e2e.js",
    "gitBlob": "f674e19da52991b8ef9f017d9197a438a4a82bbd",
    "sha256": "4a7169cb64e1f22b00b50eeb0eb02e7194754001f80b0e7f78f952cb5e5f82ef"
  },
  {
    "path": "desktop/tests/workspace-panels.e2e.js",
    "gitBlob": "8cac2961f47f74149e54bc1b07897e8b610b1389",
    "sha256": "f7c931a4693baa00221c1db63c305a8e92d2d2530500b27c8ea9ad597abefc6e"
  },
  {
    "path": "desktop/tests/workspace-panels.test.js",
    "gitBlob": "68fa525dc135fd1760dab6dbd8e217117cd04ac4",
    "sha256": "95e58029bf6fc61999ff2468a6b7c003374c2386d0dca83b164a09c469708c96"
  },
  {
    "path": "desktop/src/boundary-controls.js",
    "gitBlob": "d1df64a2090719b75c09680e4fa2e5be6a0a95f5",
    "sha256": "05d31f3ae5df41f3176c1027ce574f74957062e42c44668b93393792dac47459"
  },
  {
    "path": "desktop/src/ui-icons.js",
    "gitBlob": "2f0bbfe82abfff649018204c39f48dcb14732746",
    "sha256": "43dfe1017fc088a7a0749b5803aac84907ab7b9e4d2e96b6d50ad99fcc1e1a72"
  },
  {
    "path": "desktop/tests/boundary-controls.test.js",
    "gitBlob": "270721845eb3798a111b9f5a6a5d11532590fbdf",
    "sha256": "b654bbe839e0f4cf828a1513cfad6f2d9e0e406bbc72adb23545eb7f2511aa7b"
  },
  {
    "path": "desktop/tests/helpers/boundary-controls.js",
    "gitBlob": "cd29ec8992ab8f4a93b99d1f87876400b46860b7",
    "sha256": "e7f5048f31bd7510b72bb9a7b9998eb1a0e67f1fd3e64f8a97cbe63d20c292b7"
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
