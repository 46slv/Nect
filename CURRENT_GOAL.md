# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — REQ-210 Text Tool group (2026-10-19)

Goal: Separate Text Tool activation from authoring, with held horizontal/vertical
variant selection and explicit Canvas creation using the shared Session command.
Current phase: implementation and focused verification complete; development publication.
Proven: Focused unit/DOM tests 27/27 PASS, Vite build PASS (125 modules), actual
Note Chrome isolated fixture on localhost:5203 verifies non-dirty selection/cancel,
both directions, one Undo/Redo, Redo retention and existing Text/Inspector preservation.
Evidence: desktop/docs/req210-text-tool-20261019.md and the reproducible fixture.
Next task: Publish the scoped development commit and Draft PR against the exact base branch.
Approach: codex/req210-text-tool-20261019, independent copy from
80e32d16bf3dcb59625f305284b78b85f3d19948; sole source writer.
Done for next: Remote full SHA and Draft PR verified; native/physical IME gaps explicit.
State: Text Tool/controller, shared optional creation direction, focused tests,
related E2E creation gestures and documentation only. Existing 5199/5201 untouched.
Authority: Current delegation permits code/tests/docs and dev commit/push/Draft PR.
Main merge, release/distribution and auth/permission changes remain excluded.
Handoff: CONTINUE_CURRENT_TASK.
