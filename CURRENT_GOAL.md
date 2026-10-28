# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — 2026-10-28 Shared Color identity and persistence

Goal: Give the accepted REQ-211 color design real shared identities before UI integration; never infer identity from equal display colors.

Current phase: Root is sole production writer on codex/nect-color-core-20261028. Parallel Sol reviewers/test writers are released. The user requested root implementation; Codex is validation/parallel only, up to Sol high.

Proven: PR #107 at 99d5d252 passed Windows CI 40706323357. Downloaded color-coverage/results.json proved four real Chrome raster cases, including partial alpha/clipping/order/Other; native acceptance remains NOT RUN. The next local schema-54 shared-color foundation has 19 focused tests and 70 related tests PASS plus production build PASS. Independent review found and root repaired preset validation, SVG registry loss, foreign-import ID retargeting, and legacy-ID allocation collisions.

Implementation: Shared literal colorSlots and sibling parameter colorSlotId; atomic create/rename/set/bind/unbind commands, unbind freezes resolved Color, evaluator cache invalidation, all-composition/preset validation, referenced-slot import with independent collision remapping. No Color payload metadata, color-panel UI, palette resolver, Auto Chroma or automatic coverage provenance yet.

Next task: Publish and verify the exact foundation candidate; then propagate authored paint provenance into coverage and add the panel with existing history/save contracts. Preserve the proven earlier artifact instead of claiming it validates newer source.

Done for next: Exact-head tests/CI, identity-preserving persistence and import, and no unreviewed source blocker before adding visible controls.

State: PR #107 stacked on separately frozen PR #106 (462eb8d); neither main merge nor release. Root source worktree nect-hashi-color-core-20261028. Native acceptance is an independent pending obligation; no duplicate GUI owner.

Authority: Accepted REQ-211 design and direct Nect implementation instructions allow edits/tests/development commits/push/Draft PR. No protected-main merge/release/force/history rewrite or authentication/permission/billing changes.

Handoff: CONTINUE_CURRENT_TASK. Root owns production source.
