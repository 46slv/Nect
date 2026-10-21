# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — Color value and stable property references

Goal: Extend existing Color and gradient-stop fields with saved pre-expression RGBA value, typed Color-property refs, visible Base/Expression/Result and atomic one-Undo base editing. See desktop/docs/color-value-expressions.md for adopted scope and source evidence.

Current phase: Implementation, focused source/DOM, production browser and isolated Windows WebView2 verification complete. Exact results and limitations are in desktop/docs/color-value-expressions.md. Preserve the existing Pen continuation and Image/Shape Dither behavior.

Proven: Remote-backed independent clone of https://github.com/46slv/Nect.git at PR39 feature head c3703654b1e6354067e72cc7651656d27c8b7477; branch codex/color-value-expressions. Existing Pen de2b5893 is an ancestor; no duplicate implementation. No checkout .agents/skills directory. Focused regression 104/104 passed, including existing numeric/Color/gradient/Image and Shape Dither/preset coverage. Three additional legacy tests fail identically on isolated base c3703654; no new failure. Production build (138 modules), native configuration, and custom-protocol release native build passed. Browser Color Apply layout movement was repaired and its original pointer path passed.

Next task: Commit/non-force push this development branch and create a draft PR against verified codex/image-dither-output at c3703654. Verify remote readback. Native renderer checks passed for both gradient types; the candidate processes are closed. No main merge or release.

Approach: Reuse the common Session's typed validation, stable node ID/parameter key reference mechanism and property editor. Only value/ref Color identity expressions; schema 42 gates the new syntax. No palette store exists, so palette token refs and color math remain later scoped work.

Done for next: Source/DOM/CSP/native renderer tests and file-based JSON roundtrip evidence recorded; development commit/remote and draft PR verified. Missing Windows node_repl controller is a LOCAL_WAIT for native Save/Open OS dialog interaction only; no physical-input acceptance claim.

State: Sole feature writer in C:/Users/shiro/Documents/Codex/2026-10-21/task-13/Nect-pen-baseline. All product changes are in this isolated clone. Candidate uses disposable task-owned acceptance profiles and local CDP; installed applications and other writers are untouched.

Authority: Current parent delegation and trusted user priority instruction permit this source/test/docs work, isolated native verification, local commit, development branch push and draft PR. No protected main merge, release, force push/history rewrite, auth/security/billing changes or unrelated feature work.

Handoff: CONTINUE_CURRENT_TASK
