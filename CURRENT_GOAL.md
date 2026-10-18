# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — Parallel integration trial (2026-10-18)

Goal: Integrate the parent-confirmed SVG, Graph Ctrl+A, grapheme editing,
per-operation recovery and Dither/Points work in an isolated development branch.
Current phase: Combined source, focused/browser/native verification and one
standalone trial EXE completed; private Library delivery confirmed by coordinator.
Proven: source973486c3733916e738df8e5088f50c48e4743730 on
codex/integrate-parallel-20261018. All source candidates applied without conflicts.
Dither owns schema36; other integrated lanes do not claim a new schema version.
Graph Ctrl+A owns only current-Composition Graph focus; native Text Ctrl+A stays
native. Output remains selectable and is skipped by existing node copy.
Grapheme carets map NFC layout indices to original UTF-16 boundaries; unsafe
native deletion and arrow movement are corrected without replacing native input
or history. Missing Segmenter refuses editing, and IME composition bypasses it.
Existing shaping/font coverage limits remain explicit; no new shaping claim.

97 recovery/Text/Canvas/grapheme tests and70 Dither/Points tests pass.
Six production-CSP browser paths pass: recovery11 cases, Dither, Points,
grapheme14 fixtures including actual browser Save/Open, Graph clipboard and SVG.
The recorded three Dither exclusions are pre-existing pinned fingerprints and a
Windows CLI cwd failure; this is not a full-suite acceptance claim.
Actual built WebView2, with verified socket/PID ancestry and disposable storage,
passes SVG Path/Join, Graph A/C/V/Undo, Points edit/Undo and native grapheme
Backspace/Undo/Text commit. Real native recovery equals latest JSON; a new
profile restart restores exact JSON/SVG as unbound and unsaved. Both owned
windows exited. No normal user app/profile/recovery/private-font operation.
Native dialogs and physical IME were not exercised in this combined build.
Evidence: desktop/test-results/{integration,recovery-per-operation,
dither-controls,grapheme-app,node-clipboard,svg-part}; task-local native-integrated.

Standalone EXE9,961,472 bytes, SHA256
1AF50B17350FC07C8EDF69FD520FD8E2AE7AA3280A117919A52B26014692E577.
ZIP Nect-Desktop-integrated-20261018-973486c.zip,5,248,465 bytes, SHA256
6120662CD42B4D33AD1FAD8AF8C38FC456BB24DB5D7E618174C8C33624E1E8F4.
The package retains prior public examples/licenses and adds two synthetic
Dither/Points examples. Library retention is private, not a release.

Next task: Parent delivers the integrated trial. Repeater AE-style branch
codex/repeater-ae-20261018 at8d55121f759725e8530c849d5d1dece3f98a6037
was authorized after this EXE build began and is deferred to the next batch.
Reconcile its provisional schema36 to a new version/migration at integration.
45-degree Snap is still independent; do not block this delivery waiting for it.
State: integrated source pushed; final documentation checkpoint follows.
Authority: parent-delegated isolated integration, non-force branch push, one
combined EXE and private Library delivery. No main/protected merge, force push,
release, auth/account changes or installer operation.
Gate: external review invoked, not-required; local evidence settles the bounded
integration. External Web submissions0. No remaining denied/pending/uncertain
action for this delivery. Pending next-batch features are explicit dependencies.
Handoff: CONTINUE_CURRENT_TASK; parent coordinates the next integration batch.
