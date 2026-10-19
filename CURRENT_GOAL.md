# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Fixed/responsive SVG output sizing (2026-10-19)

Goal: Offer fixed px/mm/cm or responsive root sizing on the generated SVG copy while preserving its canonical pixel viewBox and editable source project.

Current phase: Implementation, focused verification, documentation, and production build are complete; checkpoint commit and branch synchronization are the closing actions.

Proven:
- The Desktop exporter already supported fixed px/mm/cm SVG root sizes. The responsive mode is an output-copy option; numerical precision, text editability/outline policy, image embedding/linking, and presets remain out of this change.
- `exportSVG` accepts a validated `responsive` option. Responsive output changes only root width/height to `100%`; `viewBox` and SVG body stay unchanged. The UI hides the fixed-unit selector only while responsive mode is selected.
- Focused SVG, root-unit, and export-purity tests: 13/13 PASS. The live-Text CommandSession check confirms project serialization, History, Text type/content, and Undo/Redo remain unchanged by export. Syntax checks, scoped Prettier checks, `git diff --check`, and `npm run build`: PASS.
- Actual WebView2 SVG file inspection: NOT_RUN. The earlier actual PNG pHYs file inspection and device-pixel-ratio check remain NOT_RUN; no full `npm test` was run.
- The preceding PNG density commit `23dabdff50eef04d32b3a8a5c346e6d0eb56d7d1` was pushed and its remote branch HEAD was verified equal before this checkpoint.

Next task: Commit and normally push this scoped change, verify remote HEAD equals local, then resume the next export acceptance item using current runtime evidence.

Approach: Keep settings on the export copy. Use only local focused tests/build; do not install dependencies, retry network access, bypass browser restrictions, or run the full suite.

Done for next: Fixed and responsive modes share the same SVG body and viewBox; source/Undo/live Text invariants pass; the implementation and evidence are committed and synchronized on the development branch.

State: Branch `codex/nect-regression-validation-v2`; synchronized base HEAD `23dabdff50eef04d32b3a8a5c346e6d0eb56d7d1`. The source, tests, crosswalk, verification record, and this checkpoint form one successor change; generated build output is ignored. Verify final local/remote HEAD and cleanliness at checkpoint closure.

Authority: The user's latest follow-up explicitly authorizes this narrow fixed/responsive SVG output-copy setting, associated limited tests and records, local checkpoint commit, and ordinary development-branch push. The original document, Undo history, and live Text remain protected. Force push, merge, release, credentials, permission changes, dependency install, network retry, and Notion writes remain excluded.

Handoff: CONTINUE_CURRENT_TASK
