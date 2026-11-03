# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — 2026-11-03 REQ-42 Text mixed numeric behavior P2b

Goal: Connect selected-character offsets, rotation/scale and numeric typography to
P1/P2a's common input behavior and the existing Session text-values owner. Retain
mixed differences for scrubbing, absolute typing, sparse/inherited state and Undo.
Source: [REQ-42 implementation packet](https://app.notion.com/p/40cfd279a6f3811e8438c585e952c71d).

Base: main `d2119c5dc999991c123d512a3c2d6b5b2dbb44b1`, tree
`9fd22e7528b130c4e609c9e2a03cd07e1c79b7c8`, the normal merge of P2a PR #167.
P2b started from the exact P2a candidate and fast-forwarded to the identical
merged tree. Sole writer: `codex/req42-p2b-text-mixed-20261103`.

Current phase: implementation and focused acceptance. Before the change, DOM and
actual Chrome reproduce pending width/height percent typing cancelling the scrub:
the legacy comparison mixes displayed percentages with saved multipliers. The
binding now compares displayed units and converts only at the Session boundary.
The common behavior, numeric calculation, Session/core and schema are unchanged.

Proven: actual Chrome mouse/keyboard tests cover pending 125% then relative scrub,
separate Undo steps, live detached preview, mixed differences, no-op/cancel/Redo,
stale selection, focus, pinned palette, zoom and JSON/SVG persistence. Targeted
DOM adds route/source/revision/unit guards; P1/P2a and graph navigation regressions
are checked separately. Evidence is task-scoped in `req42-p2b-evidence` beside the
isolated worktree; exact candidate and CI receipt are required before acceptance.

Next task: finish focused DOM/unit, format/build and source-bound evidence;
commit/push this P2b-only branch, open a Draft PR against main and wait for its
exact-head CI. Return to parent for independent review; do not integrate P2b.

Done for next: changed gesture one Undo, pending typing a separate Undo,
no-op/return-origin/cancel preserve Redo and sparse authored state; selection,
route, source, units, Composition and Session revision cannot retarget late input.
Preserve nonselected text, inheritance and PR #162 selection/focus/pan behavior.
Kerning, asynchronous fonts, wrap/leading and other remaining surfaces are P3.
Native WebView2/physical OS IME remains LOCAL_WAIT, separate from actual Chrome.

Authority: explicit P2b delegation permits implementation/test/commit/non-force
push and Draft PR. No P2b Ready/merge, release, force/history rewrite, approval
review or authentication/permission changes. P2a integration has separate exact-PR
user approval. Parent retains Mission ownership. Handoff: CONTINUE_CURRENT_TASK.

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
