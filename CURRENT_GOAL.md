# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — REQ-31 owned handle polar controls (2026-10-19)

Goal: Add incoming/outgoing Angle° and Length to the existing Curve/Multi Path
point panel, synchronized with Canvas, retaining Vector2 JSON and existing X/Y.
Current phase: implementation and focused verification complete; publishing a
bounded development checkpoint against codex/req210-text-tool-20261019.
Proven: 31/31 focused core regression tests and production build PASS; Note Chrome
155 disposable fixture PASS for both handles, all modes, both synchronization
directions, invalid values/Escape, Undo/Redo, JSON, protection and focus/scroll.
Evidence: desktop/docs/req31-handle-polar-20261019.md and reproducible E2E receipt.
Next task: Review the bounded Draft PR at its exact remote SHA.
Approach: sole source writer in an independent copy, branch
codex/req31-handle-polar-20261019, base f35a4659d6569b13a5a5448b1526ea5fac7ab6d9.
Done for next: Preserve native/physical IME gaps and the two baseline-only DOM
harness failures; no full-suite repair or broader Inspector redesign.
State: No schema/core command change; reuse curve-point gestures and History.
Path Remap and multi-point batch remain outside scope. Old 5199/5201/5203 and
user documents were untouched. Notion writes remain deferred.
Authority: current delegation permits code/test/docs/dev commit/push/Draft PR;
mainline merge, release/distribution and auth/permission changes are excluded.
Handoff: CONTINUE_CURRENT_TASK.
