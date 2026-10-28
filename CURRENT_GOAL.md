# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — 2026-10-28 Graphic Asset Library

Goal: Deliver a useful first production Desktop vertical for visual reusable SVG/image browsing and safe placement. The user's new explicit request authorizes this separate feature checkpoint; the completed font sprint's no-new-broad-features sentence does not constrain it.

Current phase: Automated first vertical qualified; final development-branch synchronization and CI verification. Sole source owner is the isolated `codex/nect-graphic-assets-20261028` worktree at `C:/Users/shiro/Documents/Codex/2026-10-28/task-8/Nect-assets`. It starts exactly at PR105 `48e41acf51a4cb9b8ddb2b255196a9e0da17d724` and stacks against `codex/nect-font-inspector-20261028`. PR105/font worktree, main, existing native tester and Hermes remain untouched. No additional source writer was spawned.

Design: `desktop/docs/graphic-asset-library.md` records REQ-309, REQ-159 and G5 R10 readback, storage alternatives, the selected folder-browser/embed contract, original/library/document-copy editing distinctions, safe updates, schema53 provenance and explicit residuals. SVG is a first-class node using the existing editable vector-source/reference evaluator. A new layer always uses SVG → Join → Out; graph insertion defaults to disconnected with opt-in safe terminal Join. PNG/JPEG share the same browser and existing canonical PNG new-layer command. Refresh never updates placed artwork.

Proven: Full local JS suite 2447 tests, 2442 PASS, 5 existing SKIP, 0 failures (bounded concurrency, `desktop/test-results/asset-unit-full.log`). Focused insertion/provenance tests 18 PASS; Rust registry tests 7 PASS, including identity replacement, same-name sources, relative boundaries, case aliases, junction escape, size/entry/depth/total budgets and settings failure. Real headless browser `desktop/tests/asset-library.e2e.js` PASS covers SVG/PNG/JPEG browse/search/preview, exact SVG pixel, both insertion routes, Join reuse, selection, Undo/Redo, native JSON download/reopen, unsupported markup/no remote requests, stale-document race, close-while-read cancellation and browser cancel event. Evidence is under `desktop/test-results/asset-library/`; native folder-picker interaction is not claimed by browser tests.

Next task: Commit/push the coherent feature branch and create its stacked Draft PR; fresh-read remote SHA and CI results, repair any affected failure, then return the exact candidate to the parent for independent review/Terra verification. Native foreground acceptance requires fresh explicit shared-GUI availability; none was requested or assumed here.

Done for next: Exact source/remote/PR SHA parity and final CI result (or exact external blocker), passing build/format/native configuration checks, source scope and residuals reported. No main merge, release, force push, auth/permission/billing change or third-party asset/font redistribution.

State: New isolated branch only; see the Draft PR for exact final SHA/check URLs. Production JS/Tauri source changed; legacy C++/Qt folder_library.cpp is a design reference only. No user source file is moved, edited or removed. Browser directory capability is temporary; native registered roots persist separately from document Undo. Linked reload, managed packs, drag/drop, favorites, palette integration and editing external originals remain outside this first slice.

Authority: User requests relayed with trusted transcript evidence on 2026-10-28 at 08:13 and 08:26 UTC authorize design, implementation and ordinary development Git/PR lifecycle. The parent retains overall Mission ownership and independent acceptance. Notion statuses are unchanged.

Handoff: CONTINUE_CURRENT_TASK through synchronization and final CI; native GUI remains a local pending obligation after ownership confirmation.

Upstream native observation, corrected by parent: the PR105 white capture at 08:39:24 preceded the isolated WebView first navigation at 08:39:32.944 by about nine seconds. A persistent startup failure is not established. The title proves updateDocumentStatus ran, not complete rendering. No startup source changes were made; PR105 remains the clean, released `48e41acf51a4cb9b8ddb2b255196a9e0da17d724`. This asset branch does not implement a speculative startup repair or rebase. Asset native acceptance remains NOT_RUN and requires observing readiness after navigation plus fresh shared-GUI availability.
