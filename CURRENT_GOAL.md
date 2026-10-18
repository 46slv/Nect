# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — Per-operation recovery saving

Goal: After each committed document operation, request app-owned recovery saving
without a debounce delay, preserving2 generations, original files and window IDs.
Current phase: Implemented serialized latest-state queue and common Session
notification.29 focused lifecycle/Text/gesture/notification/queue checks pass.
Production CSP UI10 operation/slow-disk/failure scenarios pass; native trial next.
Proven: Idle writes acknowledge each revision; while a write is held, later
requests remain interactive, coalesce latest and drain automatically; max1 global
writer. Failure retains last saved revision and explicit retry saves latest.
Switching documents retains old pending work with separate native tokens. IME
stages/Canvas previews do not notify; cleanup precedes commit notification.
Approach: core.js wraps dispatch and commitPrepared with read-only onCommitted;
main.js observes the operation's final lifetime in a microtask, so document imports
cannot be assigned to the prior recovery identity. recovery-autosave.js replaces
timers with one active immutable request and one latest waiting state/document.
UI distinguishes requested/saving/saved/failed revisions. Disk writes are async.
Limits: Every operation requests protection; only acknowledged writes are durable.
Queued/intermediate revisions can be coalesced or lost before I/O success/crash.
Two generations remain per document, not every operation. Encoding/validation
still scales with document size. No manual-original overwrite. Existing native
recovery format/storage and authored schema stay unchanged; runtime revision
numbers belong to the current editing Session.
Next task: Finish production UI checks, commit/build once, isolated EXE operation
acknowledgements and restart latest-state equality, write failure/retry; deliver
ZIP/Library/source/hash and normal dev push/PR evidence. No mass retest.
State: codex/desktop-production-20261016 at9e4cbf9 with autosave-only WIP.
Shared main.js hunks: sessionOptions~725, updateDocumentStatus~4691,
updateRecoveryStatus~4698. core.js:constructor~4727, commitPrepared~5510,
dispatch~5553. Other writes:recovery-autosave.js, focused tests/docs/CURRENT_GOAL.
Font handoff: no font production WIP; base9e4cbf9040c6e080710ca509765c3340fec53989.
task/autosave-writer-handoff.md and desktop/docs/grapheme-probe-checkpoint.md
record task/probe-native-graphemes.mjs + native-grapheme-before-probe.json.
Other threads separately own Dither, SVG and font. Do not import their changes
without parent integration instruction; no duplicate EXE builds or source work.
Authority: User20:03 JST per-operation request plus explicit分散 instruction,
parent scopes this writer to autosave only. Edit/test/commit/dev push/PR/EXE/Library
allowed; no main merge/release/reset/auth/Notion. User window/recovery untouched.
Gate: chatgpt-web-double-check invoked for persistence sequencing; local tests
settle bounded queue/notification design, external not-required/sends0.
Done for next: Verified per-operation trial, explicit guarantee limits and clean
remote-matching dev branch. Handoff: CONTINUE_CURRENT_TASK until delivery.
