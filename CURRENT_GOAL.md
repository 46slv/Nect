# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — 2026-10-28 installed fonts and Text Inspector

Goal: Recover installed Japanese font entries omitted by folder-only discovery, retain localized search aliases, and put everyday Text editing controls first without removing advanced operations.

Current phase: Bounded follow-on to the completed two-hour PR104 sprint, closeout 07:00 UTC. Source owner Astra task 01a180d5-0527-71f1-9523-140ed437e3bd, worktree D:/Documents/Nect-worktrees/astra-font-inspector-20261028. Rust worker returned e74d63d and released ownership; Terra reviews UI and adapts explicit disclosure operations in existing browser tests. Native GUI is not claimed: Hermes may own the shared desktop.

Proven: PR104 HEAD15d52b8f63e5beb1c19d7f64e026c6406cd61c12 has all three CI checks SUCCESS. Its tested EXE126ec6e4a6f8cb12321b23952a8a4dda47f54d2eb23c8973e4734989d9839b5d has seven bounded Windows native PASS rows. PR104 body is the final source/build/evidence index; do not rerun that whole task.

This follow-on: exact Rust source harness before505/after510 catalog rows, M+1c bold/medium/regular formerly missing and now found from Windows font registrations. 17 harness tests PASS including native registry/handle checks, with serde/Tauri adapters shimmed; this is not a full Tauri build. Registered exact files remain opaque IDs with canonical path, opened handle and identity validation. Localized name metadata is bounded and validated end-to-end. Focused unit/DOM8 PASS; real Chrome rich-text, numeric-race, installed-font switch/history/reopen and Japanese/fullwidth alias search PASS. Text Inspector actual 1280/1024px screenshots and disclosure operations PASS. M+1c regular loaded from its existing local file, rendered Japanese text and reopened serialized state in headless Chrome. A discovered font chooser input-detachment bug with Align open reproduced as no font asset after selection; the same file chooser case passes after preserving its input. No font files are distributed.

Next task: Read this branch's Draft PR for final HEAD, aggregate results and CI. Complete Windows build/CI, then use the existing native tester01a180bc-2609-7762-b126-cf6ea33ae709 only after GUI ownership is available. Verify new EXE SHA and actual installed-font catalog selection, Japanese edits/history/Save As/reopen and Inspector controls. Do not reuse PR104 native PASS for these later product changes.

Approach: No new broad features. Keep default font/size/alignment/spacing visible; advanced controls remain reachable through More typography, while font status and source-return navigation stay visible. Explicitly open the disclosure in affected browser tests, retaining semantic assertions.

Done for next: Terminal candidate CI and new native evidence, or exact blocker/NOT_RUN with candidate SHA. Japanese font complete coverage is NOT claimed: variable/color/large static fonts retain existing capability limits; actual OS-font route needs its own coverage.

State: branch codex/nect-font-inspector-20261028 based on PR10415d52b8; Rust integration0b08706. Original main9ea7ea5 and D:/Documents/Nect untouched. Pending CI/native acceptance is local to this follow-on. Detailed final counts, checks and screenshots are indexed in the Draft PR; test-results retains local logs. Original old ZIP and PR104 EXE are different acceptance candidates.

Authority: Current sprint authorizes edits/tests/development branch/commit/non-force push/Draft PR. Follow-on user intent relayed in task01a1812d-21a7-7771-8657-276c088b4002 asks installed-font differences and reduced Text Inspector clutter. No main merge, release, shared-history rewrite, settings/auth/billing changes, font redistribution, or unrelated GUI mutation.

Handoff: CONTINUE_CURRENT_TASK through bounded closeout, then resume this exact checkpoint for pending CI/native checks. No second source or GUI writer.
