# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — Per-operation recovery delivery

Goal: Request recovery after each committed operation, without debounce, while
preserving original files, two generations and document/window identity.
Current phase: Complete implementation, production/native acceptance and Library
upload. Final normal dev push/PR record closes this autosave-only writer scope.
Proven: source d8b847046f1f03af3c2a0cd784dea449208d1b3c.29 focused lifecycle,
Text/gesture/notification/queue checks pass. Production CSP11 scenarios cover
Inspector, node add, paste/delete, Undo/Redo, Canvas release/preview exclusion,
Text commit/IME draft exclusion, slow I/O latest drain, failure/retry and import
retaining the prior document's recovery identity. I/O fixture explicitly stubbed.
Actual EXE unstubbed:13 checkpoint receipts, real restart1, full latest JSON/SVG
equality, embedded data, manual original unchanged, two generation files,
write failure shows failed/current ack revision and retry saves latest. Graph
pointer preview not stored.10-command burst reaches latest r19, retry r20; restore
new Session r0 has identical full document/SVG. pageErrors[]. Owned EXEs stopped.
Evidence: docs/recovery-autosave.md; desktop/test-results/recovery-per-operation;
task/native-per-operation-recovery/receipt.json, expected.json/svg, screenshots.
Approach: core.js read-only onCommitted wraps dispatch and commitPrepared; UI
microtask observes final document lifetime. No timer. Controller has one active
immutable request and one latest waiting state per document, globally serialized
within the controller; existing native mutex/unique IDs protect native ownership.
Successful runtime revision is published only after the native write returns.
Limits: Every operation requests protection, not every intermediate revision is
durable. Burst states coalesce. Only successful acknowledged state is protected;
crash can lose pending/in-flight edits. Encoding/validation scales with document
size. Typical1MB case171–237ms incl. automation/verification is not a speed bound.
Native storage format/authored schema unchanged; revision labels are per Session.
No manual-original overwrite or existing recovery-file deletion. Two generations
are per document; global old-directory pruning remains absent. Physical IME/
high-DPI acceptance unverified. Existing18 examples retained,1 new verified.
State: codex/desktop-production-20261016, built source d8b8470.
EXE9,957,888 bytes, SHA256
5C9F62DD70E5BE0F39E2B212CAB4B21D9F3EB1C35468B789ACC6DFB47B2C845E.
Library libfile_6fcd4f86ed14819190dd2013813d897f, version0/xattr applied.
task/Nect-Desktop-trial-20261018-d8b8470.zip,5,118,794 bytes/91 entries,
SHA256 055AEF3F0B5A16A0B319B7F82F226B5056E11F38F68998E98CFD7B6B3595BE59.
Shared UI ownership: main.js sessionOptions~723/onCommitted, document status~4685,
recovery status~4694. core.js constructor~4727, commit observation/entrypoints
~5510/5553. Also recovery-autosave.js, focused tests/docs/CURRENT_GOAL. The final
delivery commit includes one extra import-lifetime test; binary source is d8b8470.
Next task: Parent integration owner handles Dither/SVG/font separate worktrees.
This writer must not integrate or resume those features without explicit direction.
Font WIPなし: base9e4cbf9040c6e080710ca509765c3340fec53989; probe-only handoff
in docs/grapheme-probe-checkpoint.md and task/autosave-writer-handoff.md. No font
code patch. Original task/probe-native-graphemes.mjs/results retained.
Authority: Latest user per-operation request and explicit distributed-work scope.
Edit/test/commit/dev push/PR/EXE/Library permitted; no main merge/release/auth/reset/
Notion. No other worker feature imported. User window/recovery untouched.
Gate: double-check invoked; local evidence settles queue/notification boundaries,
external not-required/sends0. No denied, pending, uncertain action or blocker.
Done for next: Confirm Library/source/hash report, normal push/remote match/clean
state. Handoff: NEW_TASK; completed autosave scope, parent owns further routing.
