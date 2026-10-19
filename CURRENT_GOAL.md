# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Composition view-state isolation (2026-10-19)

Goal: Preserve independent editing and viewport context for each outer Composition while switching through the current header selector. Keep view state window-local, filter stale IDs, avoid document History/dirty changes on switch, and reset old project state on Open/New/recovery.

Current phase: Implementation and focused checks complete; local commit pending. The production core factory and real browser/native UI paths are NOT RUN because pako is absent. The header selector exists; the planned keyboard-accessible tablist does not.

Proven:
- 33 focused model/import/owner/view-state/action-contract tests PASS.
- Syntax checks and git diff --check PASS.
- In-memory state is keyed by stable Composition ID and includes active Layer, node/artwork/point selection, selected Guide, Canvas zoom/fit/pan, graph zoom/scroll, tool, and node-viewer bindings.
- Restore filters Layer/Node/Guide/Viewer/point IDs against the active Composition; removed Composition states are pruned and new/imported IDs are cleared.
- Project replacement clears view snapshots and ID-keyed view details; the owner switch remains revision/history-neutral.

Next task: Commit locally. When an authorized dependency/runtime is available, exercise real UI switching, Undo-driven active-ID changes, project replacement reset, and active-ID safety. The tablist UI is a separate outstanding connection.
Approach: Review the focused source/docs diff, commit without pushing. Do not retry npm/network. Keep core/UI acceptance marked NOT RUN until executed in an available local runtime.
Done for next: The view-state connection is committed, limited tests pass, and remaining UI/runtime gaps are recorded accurately.
State: Branch codex/nect-regression-validation-v2; parent HEAD 9d9bd41d755b355c7fcf85dd2e61480d8828443c; this checkpoint commit will be the branch tip after local commit. No push. Per-Composition view state is not persisted; the Composition tablist remains future UI work.
Authority: Current user instruction authorizes this implementation, focused tests, related docs, and local commit. No npm/network retry, push, upload, account/permission changes, force operation, main merge, or release.
Handoff: CONTINUE_CURRENT_TASK