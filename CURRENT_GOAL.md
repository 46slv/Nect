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
Proven: Frozen trial checkpoint 182ab490f27b6b7d13d5252c7f75971a33ed1c6f contains 19 modifier additions. Portable browser ZIP was delivered on 2026-10-17 at 07:49 UTC; exact frozen source and package hashes were verified. Browser startup, physical IME and native GUI remain unverified. The earlier native Windows trial remains 7496aee. Approved official Rust setup and an isolated exact-checkpoint Windows build are underway separately; no native package success is established.
Current source: Instance on Points is implemented as the twentieth addition: translation-only copies of Source at Points anchors, geometric-bounds pivot, independent instance groups and bounded preflight. Helper/core/DOM/bypass focused checks total 59 passing; Web build passes. The frozen trial is not changed by this advancing source.
Next task: Publish Instance on Points, then implement the explicitly requested gradient-to-black-dot halftone and ordered dithering with a shared pure luminance sampler. Separate helper ownership covers sampling, marks and mask clipping; dot owns integration. Luminance-to-other-shape mappings remain future candidates. Preserve bounded synchronous evaluation and source geometry.
Approach: Common Session commands, explicit parameters, deterministic identity and bounded output. One writer per source scope. Use focused tests and one build per integration slice. Continue development while Shiro uses the frozen trial.
State: Cloud snapshot nect-dot-takeover has no Git metadata. Publish changed source through expected-SHA development-branch updates after focused checks, never protected-main merge or release. Other stopped missions remain stopped.
Authority: Existing Nect implementation/tests/docs/dev-branch/push/Draft PR authority applies. No main merge, release, force/history rewrite, authentication, permissions or billing changes. Other stopped missions remain stopped.
Handoff: CONTINUE_CURRENT_TASK. See desktop/docs/editing-refinement.md and requirements-crosswalk.md. Pending interpretation of “パンク” is not a global block on independent modifiers.
