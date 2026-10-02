# Nect platform and real-GUI qualification — 2026-10-02

This receipt preserves distinct source checkpoints and evidence surfaces. It does
not claim an all-green aggregate, whole-Requirement completion, or release approval.

## Windows automated checkpoints

- `bba27ee8c5b2e7def64310f9b8efdd7aeb48e04b`, tree
  `c47824d142b28cefe64a4749506d0251a128abd0`: Release/deploy PASS;
  full72 CTest71pass/0skip/1fail. See `windows-verification-receipt.md`.
- Utility candidate `cb864804aaccfe73b205405db5185174c44e81e6`, tree
  `53bfc5c96c524f209f7a19b2cacbc7637e355cee`: Release/deploy PASS;
  full74 CTest73pass/0skip/1fail. Dedicated Utility278/279 checks at DPR1/2
  include capture-file save; existing `window_interaction` PASS. Remote branch
  `dot/windows-utility-icon-20261002` points directly to the unchanged candidate.
- The remaining `folder_library_contract` failure is an owned registry scratch-key
  creation denial. Separate diagnostic confirms access denial; no security/ACL or
  assertion change was made. It remains FAIL, not SKIP/PASS. No product repair was
  isolated from that environment failure.
- Actual Utility checkout was the fresh task-3 workspace; predecessor task-2 was
  read-only. An earlier inference from unchanged task-2 was incorrect and explicitly
  corrected. The actual task-3 branch, clean state, result and full-log hash were
  independently read back after completion. Requested cwd is not proof of the actual
  writable consumer workspace.
- Utility full-log SHA256:
  `b255d07c57879e7582ec9065299415aa70ca3693020be1213155cde1cabb6d69`.
  Captured glyph/state cells were visible; caption fonts rendered as boxes. Native
  Windows GUI acceptance was NOT_RUN because the worker lacked that CUA capability.
- Requested worker model was Luna Max; the effective runtime label was not exposed
  for independent verification. Source/build ownership was released at terminal.

## Actual own-cloud GUI checkpoint

Source `740cd0b48f258fd95fe4419bd61e6b0127bf1876`, tree
`7df324245c55aeb6fd200106cceab9320b36107b`. The copied immutable desktop binary had
SHA256 `872b0ad3ec830b5ffb6cf1df41946419cda4cf9074b6bbf2e4f36cd80d24bd82`.
The Linux/Xfce desktop was1364×1024, DISPLAY=:0. CUA observed real application
windows and performed native input; this was not an offscreen test replay.

Normal1180×812 and125%1250×813 windows passed these bounded observations:
- Persistent distinct Guide/Grid/Snap icon ON/OFF, independent state and stable
  observed bounds; actual hover/tooltips, accessible names, keyboard focus/Space
  and Snap menu parity.
- Fit, Setup and Keys actually opened. At1000 logical width, Utility horizontal
  scrolling preserved reachability. View-only interactions retained revision0.
- Polygon and Star numeric725 displayed authored725 and modulo5. Actual clockwise
  drags changed geometry/numeric value and exactly one revision; one toolbar Undo
  restored725 for each. Source/object identities survived native persistence.

Normal view-only Save As was independently compared with the baseline: exact
SHA256 `524562da938c4698962deacb79a09bb6f44a8d081e0446d1545ca58c8c3fe0a8`.
Normal authoring output and125% authoring output both have SHA256
`4e6ed8c0f1505b72c626a8986a1664a7351d8edc680430ec856244bebf317ed5`, containing
both725 literals with original `gui-polygon-source` / `gui-star-source` IDs.
High-DPI view-only native Save As was not separately repeated; its revision/state
checks and normal exact-byte proof remain distinct.

An extra150% case required1500×975 physical at the existing1000×650 logical
minimum, wider than this cloud display. It is partial environment evidence, not a
source defect conclusion;125% supplied the fitting high-DPI qualification case.

GUI NOT_RUN: Escape during held drag (supported drag action is atomic), disabled
state, binding/expression, stale-context and no-net negatives. Dedicated automated
checks cover separate cases; do not claim they were manually repeated. Windows-
specific, third-party-host and subjective acceptance are not established by Linux
GUI observations. Screenshots were captured/reviewed in CUA, but no durable exported
screenshot artifact is claimed. All owned Nect windows were closed and runtime released.

Ignored detailed receipt `build/d0-evidence/gui/objective-gui-receipt-740cd0b.json`
SHA256 `4f1f5052259d9049ab3ffb995c606feafd46fd81b2882f3132cee3a1bd50d6f7`.
All three run app logs were empty. Baseline fixtures and final saved-file hashes
were independently checked by the integrator.

## Formal MCP classification

Source review at740cd0b confirms the six passing Windows MCP contracts are genuine
MCP2025-06-18 JSON-RPC stdio integrations. They initialize a separate Python sidecar,
exercise tools/resources and forward into a separate desktop-owned Host/Session
through a Windows named pipe. The backend JSON-lines IPC alone is not MCP.
Existing tests include identity, source/save and several cold-process reopen cases.

A separate named commercial MCP client and exhaustive protocol conformance were
not tested, but no new generic gate of that kind is required by current REQ-60/61/62,
which are already VERIFIED. Do not erase proven formal integration by describing
these as merely MCP-named tests. Resolve/OFX is a separate external-host domain.

Explicit D0 wire gaps identified at this checkpoint: Template Artboard duplicate,
scoped Guide alignment with `guide_artboard`, and inherited `grid:` alignment/
distribution. A separate test/docs-only parity packet addresses those cases; its
new formal Windows execution must be recorded separately. Guide committed update/
override commands already cross MCP; pointer gestures are not a new MCP operation.

## Later Windows primitive qualification

Exact740cd0b/tree7df3242 was verified in a fresh clean task-5/Nect-windows checkout.
Release/deploy PASS; primitive normal/high-DPI2/2 and selected regression19/19 PASS.
Full76=75pass/0skip/1fail, the same known registry scratch-key denial. No source
repair or security change. Runtime model label was unavailable; native Windows GUI
was NOT_RUN. Source/build ownership was released, with all build/test commands ended.
Qt deployment warned about missing translations and unset VCINSTALLDIR.
Full log SHA256 `6e3164f91d4c7ca9429edd40657f4368ca2e2106058f5509f14890f34bbe1809`.
Build/deploy SHA256 `73620f71347b67569b1fbbd058a4f8f1a7e9a20120aee113afe1b9b61cb99887`.

A fresh task-6 began setting up12b5863 but shiro-WS went offline before any build
or test. Its staged source tree matches25b5502, while reconstructed commit4cbf025
is not the canonical requested SHA; branch creation and metadata reconciliation
remain pending. No result or test log exists for that candidate. Resume the existing
incomplete task only after connection and exact workspace/state reconciliation.

## Repeater actual own-cloud GUI replay

Exact12b5863334c891efc302861f28b9285c87b4a174/tree25b55029d6370de1a5c9ce13542bc9913d5c7a73
was replayed with copied immutable binary SHA256
eeee2731a0e610e79aa077a92c50fdf738f858835e54540867715f9ca74d30f2.
Actual Linux/Xfce GUI at normal1180x812 and125%1250x813 passed bounded Repeater
zero-at-top, signed/multi-turn numeric and modulo display, actual drag/geometry and
single Undo, repeated drag, dirty focus-out selection change and same-object reselect,
primitive +X-zero coexistence, close and fresh-Host reopen. The saved native file
preserved document/source/operation identities and exact rotation1080.75.
Final native SHA256 `ee88e96852927214d7a3437806ea572bb5d1cfbafd50408fb99042b7c6f4c335`
equals the pre-reopen bytes after the high-scale replay and Undo actions. The integrator
read back this hash and verified both runtime logs empty. All owned Nect windows closed.

Ignored receipt `build/d0-evidence/gui-repeater/objective-gui-receipt-12b5863.json`
SHA256 `0acc37f35841b5a3073c29d5856aad490d694c55a0e18b43eba26d48ce351809`.
Screenshots were viewed in the CUA transcript; no durable image files claimed.
Held-drag Escape/range recovery, Windows GUI and subjective acceptance were NOT_RUN.
This does not automatically qualify later point-angle code.

## Windows Repeater and D0 MCP wire completion

Exact12b5863334c891efc302861f28b9285c87b4a174/tree25b55029d6370de1a5c9ce13542bc9913d5c7a73
completed in the clean isolated task-6 checkout after canonical commit metadata was
reconciled. Release/Qt deployment PASS; focused Repeater/primitive/Utility normal and
DPR2 tests6/6 PASS; formal MCP Template contract PASS both focused and full. Full78:
77pass,0skip,1fail known folder-library NativeFormat registry scratch-key denial.
No source repair, assertion weakening or security change. Source/build released.
The early Python3.13 configure stopped before compilation; explicit native Python3.12.11
fixed configuration. Qt translations/VCINSTALLDIR warnings remain documented.

- Build/deploy SHA256 `0e0c6850eb54a7c9701b16a2f912483fe9599cbbd6556565563f7266a0c001ca`
- Focused6 SHA256 `dd17226c90857e10fb1627224a0ae7b5ed104e26c64df8a7f03ddd396498c441`
- MCP Template SHA256 `e399f5439e4256ae945342ff9a5b0d3cdb78d88730379dee853520968098f3f0`
- Full78 SHA256 `566c36ec4a53deb2b098d0cea7143a6e7af47f42fc68ee95aa87005b96c709b2`

Native Windows GUI NOT_RUN; effective model label unknown. No CMake/CTest/Nect process
remained; resident Visual Studio MSBuild workers were left untouched. This closes the
new12b5863 D0 wire cases, not newer point/batch candidate verification.

## Single-point actual own-cloud GUI completion

Exact774a12ce2d2f94c5faef77a590c1f1fe50bdf633/tree2bee9c91ee8de6ad2d359e5f6fe6f28c010e1672,
copied binary SHA256 f7d7404a97362320fbc8e734a51d2f9cc07cecab8b9cdf1a48251318308c3905,
passed bounded actual Linux/Xfce GUI at normal1180x812 and125%1250x813. Generated
Polygon and authored Curve incoming/outgoing exact numeric, modulo, actual drag,
corresponding geometry, one Undo, zero-length editing, no-motion safety, close/reopen
and stable native identities passed. Normal-scale dirty numeric selection change
committed only the original point. Three normal/reopened files have the same SHA256
`54c5a3d6595f56248082bf53e20709e60057f44d3219ac7333f9961126f4139c`, independently
read back by the integrator. Both logs empty, owned windows closed.

Receipt `build/d0-evidence/gui-point/objective-gui-receipt-774a12c.json` SHA256
`68f26978338103e0376e50620978ef340d81ca5b5d0812cf116fa72cd2d8a308`.
Screenshots viewed in CUA transcript, no exported files claimed. Held-drag Escape/range,
continuous preview capture, driven/stale/no-net GUI negatives, other generated types,
high-scale dirty interruption, Windows and subjective acceptance remain NOT_RUN.
This evidence does not automatically qualify the later batch-point adapter.

## f24 point-batch platform findings (2026-10-02)

Exact `f24e33c4006bda894101d5276e5f68a14e9aeca0`, tree
`659bbddff437d6f11606b05ee13c91a59cc560d2`, was qualified separately:

- Windows task `01a0fb01-1c09-7206-83da-609e55a9e0bb` verified all327 source blobs,
  configured82 tests, and passed six available standalone contracts. Release desktop
  build failed with MSVC19.29 C1001 in the nested generic point-membership predicate
  in `Window::add_multi_point_angle`. Desktop/deploy/UI/MCP/full82 stages were NOT_RUN.
  Source and flags were unchanged; writer ownership was released. Build-log SHA256
  `064fd6e30e72971b6ed2b517749174a19bfe6775e0b975766b04138177a10932`.
- Own-cloud Linux/Xfce real-GUI replay passed bounded normal-scale batch numeric,
  relative drag, dirty-selection ownership, no-motion, multi-source and one-Undo checks.
  Sixteen saved snapshots independently passed native0.77 validation. At125%, a Mixed
  incoming Polygon selection starting5/365/725 produced a common delta approximately
  -5.97432328648 on a sampled quarter-circle pointer path, while a Common selection
  produced+90. This is a recorded GUI finding, not a Windows result. Screenshot-based
  centers are estimates; no held-phase widget bounds were exposed.

Frozen GUI binary SHA256
`249363b4b0e25576df21e20ef2f3405a2a7e02bf03d9e497913dd64ec0c16f11`.
Receipt `build/d0-evidence/gui-batch-point/objective-gui-receipt-f24e33c.json` SHA256
`843ce26ae320547be41d3a13eb6010e294256eef493b47d2636cf7a4a0df227c`.
All owned GUI windows were closed. Source patch556b122 uses explicit membership loops
and stable-height batch layout, with automated geometry regression evidence recorded
separately. These changes do not close the findings until exact repaired-platform replay.

## Repaired point/primitive batch actual 125% GUI (2026-10-02)

Exact local UI candidate `556b122a47248a571e93f67088248ac0e88c731d`, tree
`3ad7f2091f2ae16c41506defedafa06d032e66de`, copied desktop SHA256
`2e8c7d23f9ff9639b68ed70034e356772c63070014a4e0a6a608da6fcb0d234c`,
passed bounded actual Linux/Xfce125% replay on1364x1024 display, client1250x813.
Point Mixed incoming5/365/725 became95/455/815 under a native quarter-circle pointer
path, while Common45 became135 using the identical global path. Mixed outgoing also
added exactly90. Fixed visible incoming center remained approximately1127,530 across
Common/Mixed state. This closes the sampled f24 mapping discrepancy on the repaired
candidate; it does not establish its original cause through held-phase capture.

Whole-object Polygon+Star Common[-90,-90] became[0,0]; Mixed[-90,270] became[0,360]
under identical quarter arcs. Absolute45 and relative+=10, one document Undo,
zero-net unchanged revision, stable unrelated fields/identities, and fresh Host
close/reopen passed. Root independently read all22 artifact hashes, native0.77 JSON,
and every documented Undo/zero-net/reopen byte-equality pair. Both owned windows closed;
application log remained empty. Screenshots were viewed in the native CUA transcript,
not exported as durable image files.

Receipt `/workspace/shared/nect-gui-repaired/objective-gui-receipt-556b122.json` SHA256
`8149631a25a8fe266913941e2bb3737db6b136d3fad6ffe1b50b76c81a472b51`.
This UI candidate is included unchanged in remoteacc1534, with separately qualified
storage Q1 fan-in. It is not evidence for later Repeater-batch edits. New100% GUI,
150% (display too narrow), held-drag/Escape/range interruption, complete driven/stale
matrix, Windows GUI and subjective acceptance remain NOT_RUN for this exact candidate.

## acc1534 Windows qualification and caption-fit repair obligation

Exact `acc153464122beababbe47d7815bc89b0a39fd49` / tree
`ea9a64597ff57ad5880ab0ca966d2fe6689561d5`, clean task-10/Nect-qual2 checkout,
passed all-target Release and Qt deployment on VS2019/MSVC19.29.30137. The previous
f24 C1001 compile failure is resolved on this candidate without source/flag weakening.
An initial build session lost its completion status; the worker first confirmed no
active compiler/linker/CMake job, then completed an incremental continuation with exit0.
Desktop SHA256 `a18c833e67944e482ff01f118f3e01355781cdb4258b40786110bb75d4d78f54`.

Focused angle9/12 passed. Three primitive batch normal/DPR2/1.25 cases failed the
Common caption fit assertion. A separate ignored diagnostic copy against pristine
Release libraries isolated heightForWidth124px versus fixed80px label/row; center,
geometry, caption markers, parsed delta and full-precision accessibility passed.
The visible caption was `Common angle · 3 objects · +X zero · relative Δ +29.74488°`;
accessible delta stayed `+29.744881296942225`. This is a real portability repair
obligation, not an assertion to relax or a contradiction of the bounded Linux replay.

Storage/protection/formal MCP Template3/3 passed (verbose78 storage/180 protection
checks). Full86=82pass/0skip/4fail: those three captions plus the known NativeFormat
registry scratch-key denial. No ACL/security change. Native/SVG smoke passed, while
its MCP/GUI flags remained false; genuine MCP evidence comes from its separate
contract. Default fault instrumentation stayedOFF; Windows opt-inQ1 and nativeGUI
were NOT_RUN. Source/build ownership released, no owned build/test/app process left.

Windows task `01a0fb70-c778-7111-9a5c-29d0d3993090`, `build-win/evidence/` log hashes:
- Release continuation: `6581b38bc78f821babb03eeb5589d77fcf0febcba70afdc9c187b8032f9482c6`
- Focused angle: `997a12656c7193d17d661f2b02b8d0af6d3a4d4a812d7f21f02b9271b0b90a92`
- Full CTest: `1b9f2fceb61882318bf908498219ba582867775858b6c822973429b93050057f`
- Smoke: `aca3430da828d0dd81892d6a3917f6a6247d42fa9cfc57bbaba859e00a993b4c`
- Diagnostic: `20166564900946f992a3db72b2e69800b92b6d6ce409959f2ba738b503889049`

## Repeater batch actual 125% GUI (2026-10-02)

Frozen local source8ebbd9cd8c6aa7f0a32e407938d6cdcd085a3f4d/tree
daaa168838e6cabcad9bcf053f0f2b65888b1c5d, desktop SHA256
`2aed0b2fc8bbe64b681674f2bd70c6338c0556a05a4e8dea03069d41c0f9b5d3`,
passed bounded actual125% Linux/Xfce replay. Two distinct same-slot Repeater operation
IDs on Polygon/Star objects retained independent copies/position/source identities.
Common[0,0] became[90,90]; Mixed[-355,725] became[-265,815] with a calibrated native
clockwise top-to-right quarter arc. An initial screenshot-estimated path gave equal
94.76364169072617 and is preserved as a noncardinal sample, not labeled a quarter pass.

Absolute1080.75 and relative+=10.25, modulo-only indicator, zero-net unchanged revision,
one-Undo, native close/reopen and all unrelated authored fields passed. A full native
clockwise loop saved[5.000000000000114,1085] from[-355,725]; the floating residual is
explicitly retained, not rounded or described as exact decimal5. Shared point/primitive
regressions passed, including incoming Common noncardinal+29.744881296942225 and Mixed
+90, with correct target isolation and Undo. Actual idle captions fit; held/live delta
caption fitting was NOT_RUN because the native drag API is atomic.

25 saved snapshots passed native validation. Root independently verified all hashes,
native0.77 JSON and9 exact canonical byte pairs. CLI-normalized static fixtures include
one terminal LF while GUI serialization omits it; strict Undo comparisons use already
GUI-saved canonical files, and the one-LF difference is separately documented.
Five owned Host sessions and run-created windows closed; original windows preserved.

Receipt `/workspace/shared/nect-gui-repeater-batch/objective-gui-receipt-8ebbd9c.json`
SHA256 `bb3e449d3794148be193d2cdeb0e0e2e1a5403ac80bdd1f1755f964cb943b095`.
No held-Escape/interruption/range recovery,150% display, Windows GUI or subjective
acceptance is claimed. This source is included in remote46e84fd, not later Inspector
feedback changes. Windows caption fitting still requires its pending independent exact-candidate run.

## 46e84fd Windows requalification and measured caption copy

Exact46e84fd72988561b2ea04b9da8fcadf31332e87f/tree89bf93f07f01cfcd0e3aa373ea156a8979b23e8b
remained clean during Windows requalification. Desktop Release/deploy passed, but the
all-target build found a Windows-only test error: `mixed_domain` was undeclared in
repeater_batch_angle_ui_tests.cpp:183. Its intended exact ordered Path/Text Ref vector
is `expected`, declared at161. All three Repeater cases were NOT_RUN. Full89 therefore
reported82passed/4failed/3NotRun, not a full pass. The three primitive caption cases
still failed after width-only repair: label98x80px, heightForWidth124px, row width148px
at all three scales. The fourth failure remains the registry scratch-key denial.
Storage/protection/formal MCP Template3/3 and native/SVG smoke passed. Source/build
released without tracked edits or OpenCode calls.

Receipt `qualification-46e84fd.md` in the isolated Windows evidence directory SHA256
`da2f242c089b81d441d6ef38c9a5a1c41ddf8f2c048ea97d16bd8a0a25164f4f`.
A subsequent diagnostic against the exact production label measured Yu Gothic UI9.75pt,
17px line spacing. Three explicit lines fit98x80 at DPR1/2/1.25:
`Common · 3` / `+X zero` / `Δ +999.9999°`, and
`Mixed · 32` / `top zero` / `Δ -999.9999°`.
Common count12 also fits. Maximum measured compact line width78px; heightForWidth98
was80px. Fullnoun first lines such as `Common · 3 objects` measured117px (115px highDPI)
and did not fit98px without wrapping. Full identity and exact delta remain accessible;
visible text can omit the redundant noun while retaining Common/Mixed, count, direction
and signed delta. These are diagnostic measurements, not qualification of a product
patch. Diagnostic files were ignored, original source/tests unchanged, processes closed.

Metrics receipt `caption-layout-summary-46e84fd.md` SHA256
`a0b658a4f319535f7d770b8eb04a8abbbe7d7fca7409dabb5ba8ad229bad23fb`.
The next shared-helper candidate must rerun Windows fit and Windows-only Text coverage;
no width-only repair success is inferred from earlier Linux passes.
