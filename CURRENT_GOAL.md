# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — Local recovery autosave delivery

Goal: Recover editing after restart through app-owned local autosave, without
automatically overwriting a manually chosen project file.
Current phase: Implemented, isolated native acceptance passed, Library upload
confirmed. Delivery complete; next bounded font work remains unstarted.
Proven: sourcea43ee4b5de31e10d16b586aa8caf7e87741b38c6 (implementation5e4d3a6
plus guarded-Save cleanliness fix). JS9/native storage3 checks pass. Previous Text
production regression passed at5e4d3a6. Final EXE actual restart2 times: exact
committed encoding, restored JSON/SVG, embedded font759,720 bytes and image,
manual original unchanged, unbound/unsaved restore, visible write failure/retry,
previous-good fallback after current corruption. IPC unstubbed, pageErrors[].
Evidence: desktop/docs/recovery-autosave.md; task/native-recovery-v4/receipt.json,
expected.json/svg and screenshots; distributed NATIVE-RECOVERY-RECEIPT.json.
Approach: committed encoding observer,1-second debounce/10-second maximum timer,
native-issued per-document/window token, app-local recovery-v1 storage, Windows
exclusive run lease. Staged flush+atomic replace, current+previous per document.
On restart offer inactive unsaved copies; after restore preserve a durable new
copy before acknowledging the source. Source files remain. No manual binding used.
Boundaries: initial launch artwork baseline only; unsaved imports also protected.
IME/gesture drafts excluded. Dirty state and manual file binding remain separate.
No legacy C++ recovery changes or existing recovery-file deletion. No global disk
quota/old-directory pruning;2-copy retention is per document. Physical pointer,
manual IME and high-DPI checks remain unverified. Existing17 examples retained
without mass retest;1 embedded-data example checked. No schema/core/font change.
State: codex/desktop-production-20261016, sourcea43ee4b. Final EXE9,956,864 bytes,
SHA256 FC48A6646971184882401986E6FC5D41C66BC799C6F9DC94FF40F6834C65DD69.
Library libfile_df97cd0b962c81918d30b83fe1720f59, version0/xattr applied;
task/Nect-Desktop-trial-20261018-a43ee4b.zip,4,629,946 bytes/83 entries,
SHA256 FB54F1F96058ED897519293428DC3BF960773A0FAA0EC0FEED031862940CC770.
All exact owned recovery diagnostic native EXEs stopped. Live user app/document
untouched. Native v1/v2/v3 helpers had stale CDP/launcher inherited-pipe issues;
v4 uses per-phase unique ports, socket PID ownership and non-inherited launcher
stdio. Final full native run is v4, not earlier partial diagnostics.
Next task: Parent delivers this trial, then select the next bounded remaining
font requirement from current user priorities. No font change started here.
Done for next: Library artifact/source/hash report, normal dev push/PR record,
remote SHA match and clean working tree.
Authority: Parent-delegated accepted priorities; single source/build writer,
edit/test/commit/dev push/PR/EXE/Library. No main merge/release/auth/reset/Notion.
Gate: failure-recovery/storage design invoked double-check; local evidence
settles ownership and failure handling, external review not-required/sends0.
Shared debugging owner applied for harness failures. No active action denied,
pending or uncertain. No implementation blocker remains for this delivery.
Handoff: NEW_TASK; safe completed recovery checkpoint before independent font
work. Parent coordinates source ownership; do not create a duplicate writer.
