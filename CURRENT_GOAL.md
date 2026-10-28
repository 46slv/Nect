# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — 2026-10-28 Workspace panels and asset placement

Goal: Implement the user's iterative right-panel workspace and target-aware asset placement, without losing the frozen font candidate or the accepted asset library.

Current phase: hashi is the sole source writer after explicit release of pushed, clean d58e025df628344e235cc5481910ec734492eb20 by the previous Codex owner. The user requested direct implementation on 2026-10-28 10:54 UTC; Codex is limited to validation or explicitly separated parallel work. Older Sol/Astra ownership and model defaults do not override this instruction. No source worker is concurrently editing this branch.

Proven: The predecessor implemented the first SVG/PNG/JPEG folder-library vertical and catalog repairs, with provenance and safe document-local copies. Its 55b6de CI40669852840 passed. d58e025 separately darkens the graph surface; no native acceptance is inferred. Root's new bounded workspace module adds a thin right panel bar, pointer/keyboard Layers height adjustment saved only as workspace preferences, persistent Layers + Inspector, and an asset portal above Inspector that reserves its own sidebar space. The existing live asset DOM stays outside render() replacement so selection, asynchronous reads and file chooser identity survive. Focus and modal Escape ownership are guarded. Root local focused tests21/21 and production build passed. Browser E2E is written but NOT_RUN: isolated Chromium failed before page startup because this executor disallowed its process socket; escalation failed in environment infrastructure. These are not application-test failures or proof of visual acceptance.

Next task: Validate this exact source checkpoint with an isolated browser on the existing Codex validation lane, inspect actual screenshots and repair root-owned defects. Keep full CI bound to the final SHA. Then implement destination-aware drag as a separate bounded checkpoint using distinct artwork/graph coordinates, cancellation and stale-target guards; existing buttons remain fallback.

State: branch codex/nect-graphic-assets-20261028, based on frozen PR10548e41acf and predecessor d58e025. Root cloud worktree nect-hashi-assets-20261028 is the source lane. Existing WS asset task01a18194-0647-7523-abde-bf4d2ef0f032 is released and may validate only. Native PR105 tester01a180bc remains a separate acceptance obligation. Shared GUI ownership must be freshly confirmed; Collectanea/Resolve work must not be interrupted. Source design and prior evidence remain in desktop/docs/graphic-asset-library.md and Git history.

Acceptance: visible usable artboard OR graph beside an open asset panel; Layers and Inspector coexist; resize/open/close do not mutate authored JSON or Undo; pending asset selection and input survive rerender; no duplicate handlers, modal Escape capture or hidden focus. Test pointer/keyboard resizing, short/narrow windows, Cancel, repeated open/close, Save/reopen and graph scroll/selection preservation. Do not claim drag, arbitrary docking, pinning, palette integration or complete native acceptance from this first workspace slice.

Authority: user 10:47 UTC explicitly requests implementation of the summarized asset/layout direction, filling ordinary gaps and asking for consequential ambiguity; user10:54 requests hashi direct lead. Development edits/tests/commits/non-force push are permitted. Main merge/release/force/history rewrite, credentials/permissions/billing and third-party asset redistribution remain excluded. Draft PR creation was previously denied by automatic review; do not retry it or bypass the denial without materially new applicable authorization. Source sync and testing remain independent.

Handoff: CONTINUE_CURRENT_TASK. hashi owns implementation; validation workers may report failures but must not edit this source lane.
