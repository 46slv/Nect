# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — Saved OS-font discovery status

Goal: Distinguish an absent source font from a failed availability check without
silently treating a stale catalog as current or substituting another font.
Current phase: Bounded implementation and focused tests pass; exact-source trial
build and narrow isolated native availability check are next.
Proven: Atc65f4e6/a43ee4b production bundle, a known available modern face followed
by a rejected catalog refresh left status empty. Evidence before-fix.json under
desktop/test-results/font-availability. After fix, production CSP UI failure,
missing face, changed format, independent legacy failure and successful refresh
pass; JSON/SVG/revision unchanged, no shape or font-byte calls, pageErrors[].
15 focused checks pass:3 new catalog checks plus existing OS/desktop Text/outline
checks. DOM fixture now supplies inert recovery mocks missing after autosave.
Approach: src/os-font-catalog.js independently reads both catalogs; failed data
is null plus an error, never old availability. main.js shares the reader between
Refresh and OS picker, protects newest request, compares7 face metadata fields.
Existing native exact-face/glyph fallback guards and explicit Use DejaVu Sans
preflight are already present and are not reimplemented. Saved outline rendering
and export remain valid without the source font.
Next task: Commit/build once; isolated native ordinary/missing source status and
no-authored-change check; then trial ZIP/Library and normal dev push/PR evidence.
Done for next: Usable bounded status repair, exact source/hash and limits, clean
working tree/remote match. No broad audit or mass example retest.
State: codex/desktop-production-20261016 afterc65f4e6; only font catalog UI/helper,
focused tests/docs and this checkpoint changed. No native/core/schema/storage
change. Recovery triala43ee4b and Text/Copy/Paste/Mask/Keyer remain preserved.
Latest recovery delivery: Library libfile_df97cd0b962c81918d30b83fe1720f59,
ZIP SHA256 FB54F1F96058ED897519293428DC3BF960773A0FAA0EC0FEED031862940CC770.
Authority: Parent-delegated accepted remaining font work, one source/build writer,
edit/test/commit/dev push/PR/EXE/Library. No main merge/release/reset/auth/Notion.
User window and recovery data remain untouched. No automatic usage-reset use.
Gate: Local evidence settles the small discovery-status fix; external review
not-required/sends0. Broader font completeness/cluster/OpenType work is unstarted.
Handoff: CONTINUE_CURRENT_TASK; finish this bounded delivery first.
