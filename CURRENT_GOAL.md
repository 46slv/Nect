# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — Note recovery integration (2026-10-19)

Goal: Preserve and recover the Note's context-menu and bounded static Latin liga
work, then integrate safely over shared source 2f57db3 without merging main.
Current phase: source integrated, verified and pushed; Draft PR28 created.
Proven: Original branches recovered and remote-verified at ff301f5 and d938842
(including ef16eb4). Independent branch codex/note-recovery-integration-20261019
starts at 2f57db3ed469080c272e43b67546b1360ce26353. Both core imports retained;
schema 38 added without lowering existing schema floors. Final focused tests
75/75, additional Session regression 24/24 and Vite build 124 modules pass.
Real Chrome fixture operations cover Graph/Layer/Canvas context actions and
Undo, numeric 120/3 -> 40, Layer reorder button/Ctrl+D, input editing, locked
menus, composition import/one Undo, liga On/Off/Undo and pan. See
[recovery evidence](desktop/docs/note-recovery-20261019.md) for exact bounds.
Remote source checkpoint: e14c1c0257745330025f4215d4dcb64ba4f3a399, verified
against origin. Draft PR: https://github.com/46slv/Nect/pull/28, targeting the
shared development branch. Subsequent checkpoint edits are documentation only.
Next task: Resolve actual Ctrl+C/V and native-app acceptance gaps through the
next supported runtime path; no mainline merge or release is implied.
Approach: ordinary development-branch push and draft review only. Preserve both
original worktrees and the task-local bundles/patches. Do not treat new-PC local
f54505a as included. Do not expand to release, main merge or auth/setting changes.
Done for next: supported actual UI evidence closes the remaining acceptance rows.
State: integration source and local evidence are coherent; actual Ctrl+C/V is
unverified because Browser Use intercepts clipboard chords. Synthetic Layer
Ctrl+C/V passes. Native WebView2/IME/file dialogs/restart were not run. Parent
forwarded explicit localhost fixture loading approval; original formal file
chooser/import/Undo already succeeded and no approval retry is needed.
Authority: current delegated user instruction permits code/tests/docs, commits,
ordinary development push and Draft PR; excludes main merge/forcepush/release,
user-artwork overwrite and authentication/permission changes.
Handoff: CONTINUE_CURRENT_TASK.
