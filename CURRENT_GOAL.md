# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT

Goal: Expand practical non-destructive vector modifiers, inspired by Illustrator and Blender.
Current phase: dot implementation. Shiro explicitly requested Mirror and repeated research/additions on 2026-10-17 13:36–13:40 JST; no fixed modifier-count ceiling.
Proven: Delivered Windows trial remains 7496aee. Subsequent source fixes a9ecc62 / 411303c / 390f627 and Stroke presets 57c95e1 are preserved. Mirror now supports left-right, up-down, arbitrary reflection axis and live original retention; focused core 4/4, DOM 1/1, Join 5/5 pass. Native verification and new trial packaging remain separate obligations.
Next task: Mirror is remotely committed at b07b158abbeef8b3fb128c3ecdf636905de0b55c. Corner Round is remotely committed at b4f137d6c63724610b9f8b0d68f6897d4c163c6a. Publish bounded count-based Resample Path and transactional modifier preflight, then integrate seeded Roughen; continue research/additions. Keep existing Repeater, Offset, Boolean, Remap and Transform capabilities; do not duplicate them.
Approach: Common Session commands, explicit parameters, deterministic identity and bounded output. Root dot is the source writer; Luna research is read-only. Consolidate native handoffs; never request status-only Codex turns.
Done for each slice: Actual implementation, focused test/build evidence, documentation, exact development commit verified remotely. A checkpoint is not the end of the requested expansion.
State: Cloud snapshot nect-dot-takeover, not a full Git checkout. Base 57c95e1ab2830a878936549782c6ba07f808b717 verified remotely. Former WS writer released. Windows verification safe-checkout failure NtCreateDirectoryObject 0xC0000022 remains local; no tests ran in that attempt. Do not retry without new host evidence. Physical IME and native GUI remain open; do not claim full product completion.
Authority: Existing Nect implementation/tests/docs/dev-branch/push/Draft PR authority applies. No main merge, release, force/history rewrite, authentication, permissions or billing changes. Other stopped missions remain stopped.
Handoff: CONTINUE_CURRENT_TASK. See desktop/docs/editing-refinement.md and requirements-crosswalk.md. Pending interpretation of “パンク” is not a global block on independent modifiers.
