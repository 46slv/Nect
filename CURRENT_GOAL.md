# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — Explicit safe font replacement

Goal: Close one remaining missing-font/substitution path without duplicating the
existing DirectWrite exact-face guard or font-picker preview validation.
Current phase: Implemented and production-browser verified; commit/Windows trial
and a narrow isolated native check are next. No broad font-completeness claim.
Proven: The old clear-os-font-text removed the stored OS appearance without
testing the bundled replacement's coverage/layout. It now evaluates the complete
replacement Text before committing. Failure retains JSON, saved outlines,
revision and history. The UI names source OS face and the replacement action
Use DejaVu Sans; success names DejaVu Sans too. Supported replacement is one Undo.
Focused model/DOM12 passed, including missing replacement font, Japanese glyph
refusal and supported Latin replacement/Undo/Redo/reopen. Production Edge/CSP
tests/font-switch.e2e.js passes actual controls and exact SVG after reopen.
Keyer/Mask8 model checks passed. No native source/schema changes.
Approach: Reuse common evaluator and its missing-glyph/layout refusal; do not
introduce fallback chains or replace any exact-face DirectWrite repairs. One
source/build writer. Never operate on the user's running window/document.
Next task: Commit verified source, one Windows build and narrow native font-switch
acceptance; deliver a versioned trial and source/Library evidence to parent.
Done for next: Concrete usable checkpoint, normal dev push/PR, exact source and
binary hashes, focused evidence and honest manual-acceptance limits.
State: codex/desktop-production-20261016 at d775951; only this font-switch slice
is dirty. Last delivered source9cde8a4 and Library libfile_d60022e6d96c81918b7825b50fde0858
remain unchanged. QR libfile_d837e47e864481918f8a3bdf52a8d81c also unchanged.
Authority: Parent delegated current user font follow-up; edit/test/commit/dev
push/PR/EXE/Library authorized. No main merge/release/permission/reset use/Notion.
Double-check: Skill invoked; local critic of atomic failure, exact source/target
labels, coverage and one Undo. Consultation not-required: existing common
evaluator plus failing-before/passing-after tests settle the bounded fix.
External sends0, no unresolved saved-format or authority decision, cycle1.
Handoff: CONTINUE_CURRENT_TASK.
