# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — Node picker category scroll (2026-10-19)

Goal: Add one-level category toggles whose explicitly opened heading aligns to the
node picker list top, preserving authored state and unrelated operations.
Current phase: implementation and focused verification complete; development publication.
Proven: Independent copy/branch codex/toggle-list-scroll-20261019 begins at
e8c9db5225f6e26e36b0d69d3acb3320829b9fb0. Focused tests 18/18 and Vite build
(124 modules) pass. Actual Chrome on dedicated localhost:5201 verifies final-category
heading alignment, closed-category search, Japanese alias/Enter/one Undo, short-tail
space and stable native Left/Right collapse/reopen. Page/dialog remain at zero.
Evidence: desktop/docs/toggle-list-scroll-20261019.md; ignored local screenshots/logs.
Next task: Review the bounded Draft PR; native/physical IME acceptance remains separate.
Approach: Ordinary branch commit/push and Draft PR against the exact PR28 branch.
Done for next: Remote branch/PR HEAD verified against the committed candidate.
State: Only picker/catalog/style, focused tests/fixture and checkpoint/evidence docs changed.
Previous source worktrees, user artwork and existing localhost:5199 remain untouched.
Authority: Current delegation permits code/tests/docs, dev branch commit/push and Draft PR.
Mainline merge, release/distribution and auth/permission changes remain excluded.
Handoff: CONTINUE_CURRENT_TASK.
