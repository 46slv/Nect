# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — Multi-Composition session integration (2026-10-19)

Goal: Connect commands, chronological Undo/Redo, native/browser Open, Save,
recovery, and public load to the multi-Composition project envelope while
preserving inactive Compositions and shared assets. SVG/PNG use the active
Composition projection.

Current phase: Implementation complete; focused dependency-free checks passed.
Production core-factory runtime and UI lifecycle remain unverified.

Proven:
- 19 focused model/owner tests PASS.
- JavaScript syntax and git diff --check PASS.
- main.js uses project snapshots for persistence/recovery and active Composition
  projections for existing commands and SVG/PNG output.
- Legacy single-Document JSON goes through the core deserializer/upgrader.

Next task: Run the core factory and multi-Composition lifecycle acceptance
when an authorized local dependency/runtime is available.
Approach: Verify differently sized Compositions, shared assets, cross-tab
Undo/Redo, legacy migration, Open/Save/recovery/public load, and active
SVG/PNG output in the real runtime.
Done for next: The core owner and real browser/native lifecycle preserve every
Composition, project-shared assets, and active output sizing through reopen,
recovery and Undo/Redo.
State: Branch codex/nect-regression-validation-v2; implementation and tests
committed locally; no push performed. Required external
packages are absent; npm/network retry is excluded by the current instruction.
Authority: Current user delegation permits implementation, focused tests,
related docs, and local commit. No network/npm retry, push, restore clone,
backup, upload, force operation, main merge, release, authentication, or
permission change.
Handoff: CONTINUE_CURRENT_TASK
