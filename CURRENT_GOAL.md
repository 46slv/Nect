# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Multi-Composition session integration (2026-10-19)

Goal: Keep every outer Composition and shared asset intact while editing, switching, undoing/redoing, opening, saving, recovering, importing, and publicly loading a project. Preserve legacy Document loading, active-Composition SVG/PNG projection, and CompositionReference Layer-graph semantics.

Current phase: Project-wide session wiring, import, and per-Composition view-state work are implemented and locally committed through 8dbb051. This follow-up verified the production core and focused suites. Full browser/native lifecycle acceptance remains incomplete.

Proven:
- One authorized npm ci completed: 43 packages installed, 0 vulnerabilities.
- 64 focused core, Composition model/session/import/view-state, import-action, and document-lifecycle tests PASS.
- Direct createCompositionSessionOwner smoke against the production Core History/CommandSession PASS, including cross-Composition Undo/Redo and active-size SVG projection.
- npm run build PASS; Vite reports the existing 1.3 MB JavaScript chunk-size warning.
- Real Chrome E2E imports all outer Compositions and remaps IDs on repeat import PASS. Its Undo click is blocked by the Tools nav at 900×620; later history assertions did not run. See desktop/test-results/project-composition-import-ui.json.
- npm test FAIL overall. A representative synthetic-DOM test fails while resolving relative imports from a data-URL loader (Invalid URL); an exploratory harness workaround was reverted. Do not claim the aggregate suite passed.
- Browser Open/Save file flow and native WebView2 Open/Save/recovery remain NOT_RUN. A later local-browser attempt returned ERR_NETWORK_ACCESS_DENIED; this access denial was not bypassed.

Next task: Complete real UI acceptance for browser Open/Save, project Undo/Redo, active SVG/PNG dimensions, legacy JSON, and shared font/image assets; separately run native Open/Save/recovery where WebView2 is available. Resolve the 900×620 Undo hit-test blocker or verify the intended shortcut path in an allowed browser runtime.
Approach: Resume only in an allowed local browser/native runtime. Keep native recovery acceptance local-wait while independent code and test work remains. Do not retry npm/network, bypass browser access denial, or push.
Done for next: Verify project-preserving Undo/Redo, browser Open/Save, legacy load, recovery, exports and shared assets through their real acceptance paths, or record exact remaining runtime blockers.
State: Branch codex/nect-regression-validation-v2, HEAD 8dbb0517e7b0a9fe63d34747c53e8d99342be534, five commits ahead of origin, no push. Targeted E2E selector text now matches the visible Project composition menu item. One local E2E check receipt is recorded at desktop/test-results/project-composition-import-ui.json.
Authority: Current user delegation authorizes implementation, focused tests, related docs, and local commits. No dependency/network retry, push, clone/backup/upload, force operation, mainline merge, release, or access-policy change.
Handoff: CONTINUE_CURRENT_TASK
