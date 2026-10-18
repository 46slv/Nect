# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — Text canvas drag repair

Goal: Restore Select/Move dragging of Text/OS Font, preserving text/font/outline
data, clear text-edit distinction, locks,1 drag/Undo and transformed coordinates.
Current phase: Reproduced and fixed; production UI passed, commit/build and narrow
isolated EXE acceptance are next. Deliver this usable fix before autosave.
Proven: At9f2db9c a click followed immediately by drag opened the text editor on
the second pointer-down and left position220/240 instead of250/260. The new
pointer-up decision only accepts completed stationary clicks;>=3px movement,
including out-and-back, cancellation/stale Session/revision cannot enter editing.
Source: desktop/src/main.js. No schema/core/font/native/storage change.
Browser evidence: tests/text-drag.e2e.js, test-results/text-drag/receipt.json;
8 movement variants (normal/saved OS Text, rotation/negative Transform, zoom,
locked source position with movable parent),4 lock cases, intentional double
click/Escape, net-zero drag, exact one Undo/Redo. Text/font/OS snapshot unchanged.
7 existing canvas gesture/caret checks pass. Initial pre-fix probe is retained
under test-results/text-drag/before-fix.json. Explicit Move node stage is unchanged;
an overlapping source marker was classified as test-point setup and excluded
from final-artwork drag tests, rather than altering its intended semantics.
Approach: Reuse existing move resolver/gesture and supported graph capsule.
No conversion to outlines, new font, implicit Transform or graph expansion.
Next task: One Windows trial and isolated actual pointer/Undo/reopen checks,
including real saved Windows Text; then immediate Library/source/hash report.
Done for next: Usable verified Text drag trial, normal dev push/PR and clean state.
State: codex/desktop-production-20261016 atdf04323; only Text drag UI/test/docs dirty.
Copy/Paste source9f2db9c, Library libfile_4d2327cc40988191b52293428b3bca97 remains
delivered. Font fix0fcffc8 and Mask/Keyer are preserved. No user window touched.
Queue: next local recovery autosave (debounce, status, restart choice, app-owned
isolated storage, atomic replacement/bounded good copy, no manual-file overwrite
or existing-data deletion); then remaining font work. Desktop storage/lifecycle
read showed no autosave hook yet; legacy LiveSave still needs a focused lookup.
Authority: Parent-delegated accepted user priorities; one writer, edit/test/
commit/dev push/PR/EXE/Library. No main merge/release/permission/reset/Notion.
Double-check: Existing gate invoked; local evidence settles the bounded UI
arbitration, consultation not-required, external sends0. Debugging owner reused.
Handoff: CONTINUE_CURRENT_TASK; deliver repair before the autosave checkpoint.
