# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — Priority Graph Copy/Paste

Goal: Ctrl+C/Ctrl+V for selected graph nodes, including internal connections and
settings, across Compositions in the same open Document; one Paste/one Undo.
Current phase: Implemented; focused24 and production Edge/CSP shortcuts passed.
Commit/build/isolated actual EXE check and immediate Library delivery are next.
Proven: Copy captures authored node snapshots without changing revision/history;
Paste reuses duplicateNodes identity rules for fresh node/point/contour IDs and
internal socket/expression/Reference/Text-path remapping, relative placement and
selection. Actual SVG part references, embedded Image/fonts, Text, Composition
Reference and Mask/Keyer preserve same-document data. Graph/IME/input focus,
one Undo/Redo, locked target, external input refusal and exact JSON reopen/SVG
were checked. Context/duplicate tests preserve existing Ctrl+D and menu behavior.
Evidence: tests/node-clipboard*, tests/node-context-menu-dom.test.js,
test-results/node-clipboard/receipt.json; desktop/docs/node-copy-paste.md.
Approach: Private Session clipboard only; New/Open/import clears it. No OS or
cross-Document clipboard. Output nodes skipped with count; destination Output
unchanged. External inputs stay original in the same Composition with notice;
cross-Composition paste requires those sources included, otherwise refuses.
Missing/changed assets, missing Composition targets and cycles refuse atomically.
Existing edited Path Remap group refusal stays. No blind reference rebinding.
Next task: One versioned Windows trial; actual owned WebView2 Ctrl+C/Ctrl+V,
Undo/save-reopen checks; Library/ZIP/hash report without waiting for other features.
Done for next: Immediate usable Copy/Paste delivery and synchronized source/PR.
State: codex/desktop-production-20261016 at0fcffc8; only Clipboard slice dirty.
Previous font fix0fcffc82721c83c69a16b1b959936dc9cfc8c823 is committed/pushed,
model/DOM12 and browser checks passed; its own native check/distribution was
preempted. Owned font diagnostic PID51260 stopped; no user window touched.
Prior Keyer/Mask source9cde8a4 and Library libfile_d60022e6d96c81918b7825b50fde0858
remain preserved. QR libfile_d837e47e864481918f8a3bdf52a8d81c remains unchanged.
Queue after Copy/Paste: Text canvas drag regression (reproduce, repair selection/
editing distinction, locks and transformed coordinates), then recovery autosave
(existing LiveSave reuse, app-owned isolated recovery, atomic writes/bounded good
copies, failure status/restart choice, no manual-file overwrite), then font gaps.
Authority: Parent-delegated latest user priority and accepted recovery request.
Edit/test/commit/dev push/PR/EXE/Library permitted. No main merge/release, auth or
permission changes, usage reset, or Notion writes. Single writer remains here.
Double-check: Skill invoked, local dependency/identity/lifetime/Undo review;
external consultation not-required, sends0. Debugging owner classified browser
failure as test selection preparation, not product error; no selection change.
Handoff: CONTINUE_CURRENT_TASK; deliver Clipboard outcome before queued work.
