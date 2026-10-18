# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — Local recovery autosave

Goal: Recover editing after restart through app-owned local autosave, without
automatically overwriting a manually chosen project file.
Current phase: Authorized next checkpoint; read-only reconnaissance only.
Proven: Desktop storage/lifecycle and main.js have no autosave hook found yet.
Legacy LiveSave still needs a focused lookup before deciding whether to reuse it.
Approach: Debounce document mutations; show success/failure status; offer recovery
on restart. Keep different documents/windows distinct. Preserve embedded images
and fonts. Use atomic replacement, a last-good copy and small bounded retention.
Never delete existing user recovery data or touch the live user document/window.
Next task: Read desktop/src/storage.js, document-lifecycle.js, native
document_storage.rs/acceptance_storage.rs and locate legacy LiveSave. Implement
through existing storage authority and common Session boundaries. Verify isolated
mutation -> autosave -> restart -> restore equality and a write-failure case.
Done for next: Focused checks plus isolated real-EXE recovery, usable trial,
normal dev push/PR, confirmed Library artifact and exact source/hash report.
State: codex/desktop-production-20261016; Text source6861ee4 built and uploaded.
No autosave code changes yet. Remaining font work follows recovery autosave.
Authority: Parent-delegated accepted user priorities; one writer, edit/test/
commit/dev push/PR/EXE/Library. No main merge/release/permission/reset/Notion.
Handoff: NEW_TASK; completed Text repair delivery and context compaction mark a
safe boundary before the independent storage/lifecycle checkpoint. Parent owns
coordination; do not create a second source/build writer.

Latest delivered Text repair: source6861ee4ee98730f6b677bc2d9393d305cf2392d3,
Library libfile_55dc531b24008191ae1818877d34c278,
task/Nect-Desktop-trial-20261018-6861ee4.zip (4,095,783 bytes /79 entries),
ZIP SHA256 9B45BE5E191EDBBF85F49E285264910BAD0FC5C77C12D611E3C5AAC7CD3FB740;
EXE SHA256 2963FF1FF0E61FFC704F9FE91EA96D7E92FA12C1E9A1CC6BCF5DDB6398B2B197.
Verified:7 canvas model checks; production8 movement/4 lock cases; intentional
double-click and net-zero drag; actual isolated EXE normal/negative Transform/
real Windows saved Text pointer drag, one Undo/Redo, reopen exact SVG and unchanged
text/font/outline snapshots. Native diagnostic stopped; live user app untouched.
Evidence: desktop/docs/text-drag-repair.md and distributed BUILD/STARTUP,
NATIVE-TEXT-DRAG-RECEIPT/TEXT-DRAG-UI-RECEIPT. Existing16 examples retained without
mass retest;1 new native example checked. Physical pointer/IME/high-DPI unverified.
Schema35; Copy/Paste9f2db9c, font fix0fcffc8 and Mask/Keyer remain included.
Text gate: local evidence sufficient, external sends0; no schema/core/font/native/
storage change. Autosave is explicitly absent from this delivered trial.
