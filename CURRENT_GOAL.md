# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Composition lifecycle integration (2026-10-19)

Goal: Connect command history, Save/Open, browser Open, recovery and public load to the multi-Composition Session without losing inactive Compositions, Layers or shared assets.

Current phase: The production main-module integration path and targeted owner/model regressions pass. A local commit is next.

Proven:

- Extended desktop/tests/blank-artwork-dom.test.js to import actual main.js, CompositionSessionOwner, DocumentLifecycle and RecoveryAutosave, replacing only native filesystem/recovery I/O with in-memory adapters. This exercises the real core factory through the UI Session.
- The test uses two differently sized Compositions, shared embedded font/image assets and a non-active Layer. It verifies tab switching creates no history step; project-wide Undo/Redo; SVG dimensions and PNG sizing from the active Composition; Save As, native Open readback and Save through an existing binding; Recovery Copies restore as unsaved; browser Open through the toolbar and file input; public load; invalid JSON leaves current state intact; and legacy root-Document migration.
- node --test desktop/tests/blank-artwork-dom.test.js desktop/tests/composition-project-model.test.js desktop/tests/composition-session-owner.test.js: 31/31 PASS. Prettier and git diff --check: PASS. No full suite, dependency install, network operation or push was run.
- SVG output dimensions and the production PNG dimension calculation follow the active Composition. Real PNG raster pixels and native WebView/Tauri file operations have not been exercised here.

Next task: Commit the tested fixture and this checkpoint locally; then the next concrete runtime acceptance is native WebView file binding/reopen plus pixel-level PNG output when that permitted runtime is available.

Approach: Keep the change test-focused: the existing main/session/storage integration passed the focused paths, so no production code modification is indicated by this acceptance.

Done for next: Local commit exists; the test records the behavior and exact remaining native-runtime acceptance boundary.

State: Branch codex/nect-regression-validation-v2; starting HEAD db467691d7715b3afc5ea5c9f492aff63d775ae3; this checkpoint's test and record changes are pending a local commit. No remote update requested.

Authority: User explicitly authorized Nect implementation/tests and a development-branch local commit. Network/retry, push, restoration clones, backups and uploads are excluded. Existing access-denial boundaries remain in force.

Handoff: CONTINUE_CURRENT_TASK
