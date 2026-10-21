# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — Contour-scoped point navigation

Goal: Make outlined Text and compound points browsable by actual unit/contour membership inside the existing paged point panel, preserving stable Canvas/row selection and authored data. Scope and source evidence: desktop/docs/contour-point-navigation.md.

Current phase: Implementation, 64/64 focused model/DOM checks and actual native WebView2 verification complete on owned compound, generated Remap and owned outlined Text routes. Sole feature writer; no replacement scope or second writer.

Proven: Verified PR40 final feature head 77603a353da69c64cc119ee3e2c92089678d5ed6; branch codex/contour-point-navigation. No equivalent contour filter existed in the base point panel. Added view-only Unit/Contour selectors and stable-ID active-point following. Dedicated DOM cases cover 1,237-point bounded paging, hole/closure/group identity, selection/history neutrality, separate unit movement and point edits, outlined Text and generated Remap batch independence. Color artifact was copied and hash-verified into ../color-artifact-5c4cd2a; handoff ../color-artifact-handoff.md. No Color suites rerun.

Next task: Draft PR41 https://github.com/46slv/Nect/pull/41 is open against codex/color-value-expressions at 77603a3. Product/source e59b2f61a661d021af479e47af42173f726428cf is pushed. Verify the final documentation checkpoint remote HEAD, then return the PR/SHA. Additional trusted user authorization resolved the prior publication denial. The next feature will be separately scoped; common delta controls for selected owned points remain an unmet OUTLINE-EDIT-UI capability. No completed tests are rerun for this publication step.

Approach: Filter existing point records by saved unit/contour IDs; retain 24-row pages and source contour traversal order. Units without provenance are not invented for generated Remap. Existing Unit selection/movement and Remap batch controls remain separate. No core/schema/bake/rebind rewrite.

Done for next: Verified source, tests, exact candidate, remote commit and draft PR recorded with limitations and next unmet OUTLINE-EDIT-UI goal. OS dialogs/physical input remain explicitly outside demonstrated native renderer coverage.

State: Isolated remote-backed checkout C:/Users/shiro/Documents/Codex/2026-10-21/task-13/Nect-pen-baseline. Color PR40 and accepted product remain preserved on their feature branch and immutable local artifact. New candidate builds may reuse this task's native-target only.

Authority: Current delegated serial feature contract authorizes main.js/focused tests/docs, isolated native renderer checks, development branch push and draft PR. No protected-main merge, release, force push/history rewrite, auth changes, denied paths or disabled native APIs.

Handoff: Source and bounded verification complete; publication is resumed and Draft PR41 exists. Return verified remote SHA to parent. Preserve the sealed contour and Color artifacts unchanged; no new feature implementation is assumed.
