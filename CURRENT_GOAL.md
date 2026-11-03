# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — 2026-11-03 REQ-42 Grid opacity numeric binding P3a

Goal: Connect the existing Grid opacity number field to the common input behavior
and existing numeric-command Session owner, with 0–100 percent display and 0–1
saved values. Preserve its range slider, typing, exact values and Undo/Redo.
Source: [REQ-42 implementation packet](https://app.notion.com/p/40cfd279a6f3811e8438c585e952c71d).

Base: main `ee3e37ceeb621161a0572b79564379c635e1922e`, tree
`4af82244c97fae7b109b22f0a53ff1220403b3c2`, normal merge of P2b PR #168.
This isolated branch started at unchanged P2b head `623c7c51` and fast-forwarded
to the identical merge tree. Sole writer: `codex/req42-p3-grid-opacity-20261103`.

Current phase: implementation and focused acceptance. Actual Chrome reproduced
25% staying 25% after a 24px horizontal drag instead of reaching 45%: the field
had numeric affordance but no binding. The existing command binding now resolves
Grid opacity, converts only at its UI/Session boundary and reuses the existing
preview/cancel functions to synchronize number, range and overlay. No kernel,
Session/schema changes or new pointer-handler branch.

Proven: actual Chrome passes scrub/live preview/one Undo, range slider, native
click/type/arrows, pending typing as a separate Undo, cancellation and Redo.
DOM checks exact unchanged fractions, canonical endpoints, stale control/revision
and Composition ownership. P1/P2a/P2b focused regression and normal CI are required
for the exact candidate. Task evidence: `req42-p3-grid-opacity-evidence` beside
this worktree; original font/user document/application state is not a fixture.

Next task: finish focused tests/Chrome, format/build, source-bound receipt;
commit/push P3a-only changes and open a Draft PR against main. Record exact-head
CI terminal results and return to parent review. #168 integration/post-merge CI
is tracked separately in `req42-p2b-evidence`.

Done for next: 0–100% display/0–1 saved, peer range synchronization, native input,
pending typing separate Undo and gesture one Undo; exact no-op/return-origin/
cancel with Redo retained; stale revision/Composition/control never late-commit.
Unchanged number displays do not round-trip authored fractions through percent.
PPI, asynchronous font work, Color changes and other scalar migrations excluded.

Authority: explicit P3a delegation permits implementation/test/commit/non-force
push and Draft PR. No P3a Ready/merge/release/approval review, force/history rewrite
or security changes. #168 integration has separate exact-PR user approval.
Native WebView2/physical OS IME remains separate LOCAL_WAIT; do not repeat known
missing capability or CIM access failure. Parent retains Mission ownership.
Handoff: CONTINUE_CURRENT_TASK.

## Prior checkpoint — 2026-11-01 explicit key-color initialization infrastructure

Historical infrastructure checkpoint, superseded as the active task by P1 above.

Goal: Add developer/test-fixture-only atomic initialization from fully explicit supported versioned intent and exact fresh stable-ID/name allocation. Do not treat this as ordinary-user creation or approved product coefficients.

Current phase: Sole source lane on `feat/keycolor-atomic-initialization`, isolated worktree `/tmp/nect-keycolor-atomic-initialization-20261101`, exact base `e462c1aab68c9e531e5e64d887889b308811d548`. Independent review is read-only.

Proven: 21 new real Document/Composition tests pass, and the combined initialization/retention/Apply regression is 70/70 PASS. Exact C1 HEX/alpha0 and IDs/names; revision-zero receipt and one Undo/Redo/native round trip; legacy preservation; receipt-present refusal; fresh-only/global collisions including inactive Composition/Layer/Node/Point/Guide/Node-group; allocation/version/mapping/raw JSON/alpha/budget rejection with exact Session envelope and pending Redo; detached preparation, stale request and unchanged blank defaults. Independent read-only review found and verified repair of an inherited Array/proxy allocation-method atomicity hole: nonordinary arrays reject and the whole guarded request is detached before consumption. No remaining actionable review findings; independent focused regression 70/70 PASS and diff-check PASS. Existing prototype/retention/Apply/editor semantics are unchanged.

Approach: Closed v1 initialization request pins model2, experimental-affine-v1 and linear-srgb-straight-v1. Reuse raw lossless-JSON guard before whole-request detachment and pure createKeyColorRecipe. Append new slots and schema58 retained-only receipt in one private Session candidate. Composition proposed-ID preflight runs before inner Session commit, not the generic post-validation rebuild fallback.

Next task: Complete aggregate format/unit/build checks, independent source review, source-bound receipt and authorized development-branch/Draft PR publication; follow normal CI to a terminal result for the exact candidate tree. Repair bounded findings within this lane.

Done for next: Independent review has no unresolved actionable finding, local checks and exact-tree CI evidence are reconciled, source-bound receipt and Draft PR are available. This packet completes infrastructure only; ordinary-user creation still needs an accepted versioned initial plan/role-count mapping, especially dark-role lightness ordering, or explicit approval of a particular experimental plan.

State: Local candidate based on main after #153/#154 completion. No creation button/preset factory/coefficient editor/JSON import UX/main wiring; no sticky ownership/load solve/default change. Browser/native acceptance is not claimed; known local browser sandbox failures are not retried or bypassed. No font bytes, user documents, native app launches or denied/session paths are accessed.

Authority: Current explicit bounded implementation request permits Nect code/tests/docs, coherent commit, development-branch publication and Draft PR. Do not mark Ready, merge, release, force-push, rewrite history or submit approval reviews. Do not change authentication, permissions or billing.

Handoff: CONTINUE_CURRENT_TASK. Parent retains overall Mission ownership; this isolated lane owns only initialization implementation/verification/publication.
