# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Whole-project Composition import (2026-10-19)

Goal: Connect the existing project Import action to an atomic all-Compositions or one-Composition additive import. Preserve source dimensions, complete Layer stacks, Layer-graph references and referenced shared fonts/images. One batch is one Undo/Redo project operation. Open remains whole-project replacement; SVG/PNG continue to use the active Composition projection.

Current phase: Implementation and focused checks complete; local commit pending. Production core-factory and real browser/native import acceptance are NOT RUN because required dependencies are missing.

Proven:
- 25 model/import/owner/action-contract tests PASS.
- Syntax checks and git diff --check PASS.
- Legacy JSON passes through the existing project deserializer; the import dialog offers All Compositions by default or one selected outer Composition.
- The session owner validates/remaps the entire candidate before one project-history commit; invalid assets reject without changing project revision/history.
- Open stays on the replacement path; Add SVG is separate. Native source-file bindings are released after reading, and stale edits reject the pending import.

Next task: Once an authorized local dependency/runtime is available, run the production core factory and browser/native multi-Composition lifecycle acceptance. The current checkout lacks pako; npm/network retry is excluded by the current instruction.
Approach: Exercise all-set and single import, collisions, missing-asset atomic failure, one-step Undo/Redo, Open/Save/recovery/public load, and active SVG/PNG output in the actual app/runtime.
Done for next: The production core and real browser/native paths preserve complete source sets, assets and history; otherwise record the exact local capability blocker and resume trigger.
State: Branch codex/nect-regression-validation-v2; parent HEAD b5284afca12867824734c8b4336165d186a7e159; this checkpoint commit is the branch tip after local commit. No push. Per-Composition selection/active Layer/zoom/pan retention remains separate follow-on work.
Authority: Current user delegation authorizes implementation, focused tests, related docs, and local commit. No npm/network retry, push, restore clone, backup, upload, force operation, main merge, release, authentication, or permission change.
Handoff: CONTINUE_CURRENT_TASK