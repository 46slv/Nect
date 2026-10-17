# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — 2026-10-17 12:40 UTC

Goal: Prioritize Shiro's hands-on feedback and font/path usability before speculative modifiers. The user explicitly broadened font support on October 17; the earlier mission's narrow font Must is historical, not a reason to ignore this new request.

Published source: 09900726d6ac0981bac716dcbdfb6fb26a0e723f (schema 22). The 25-modifier catalog, tone response, graph alignment snapping, seven interaction fixes, curve-preserving font corner rounding, static family/style selection, and undoable Text → Multi Path baking are implemented. Multi Path retains independent glyph units/counters and editable anchors; Alt-click/drag creates handles. Refer to feature documents and targeted tests for exact limits, not a blanket completeness claim.

Verification: focused source/DOM checks and production Web builds pass for these slices. Native font-catalog Rust changes have not been compiled. Curved Corner Round is a bounded G1 blend with nominal strength and conservative diagnostics; it is not a certified arbitrary-topology circular fillet. Variable fonts, cluster-safe complex-script editing, RTL/fallback and broader font format parity remain staged work in desktop/docs/font-support-plan.md.

Active ownership: expandable Join inputs owns core/main and focused tests. Character alignment/equal-gap snapping has a completed design and awaits the shared UI scope. Never start another writer for the same files. The user chose Join Paths as the display/search name; retain the persisted Join Geometry type for compatibility. Preserve legacy port IDs and existing geometry budgets.

Next concrete work: finish expandable Join with a trailing empty socket and transactional bounds; then add same-Text sibling ink-edge/center and equal-gap snapping using desktop/docs/character-alignment-snapping.md. Maintained equal spacing after font/size changes is a separate persistent arrangement requirement, not implied by transient drag snapping.

Frozen deliveries: native b9fe3f64732e119b409a4761894c17f47893df40 was delivered at 10:59 UTC (Library libfile_0bf2f7e94fac81919256d212ac38fcef); browser cec110497cdf27a8a90bdf6afe282445e5cadf9e at 10:26 UTC (Library libfile_d75c0ab0205c8191af8626ec909fb4c6). Earlier packages remain unchanged. New source changes are NOT in those deliveries. Native startup/close passed, but full editing/save/IME/high-DPI acceptance is still unverified. Keep version-specific saved JSON separate.

Native update: authorized shiro-WS was offline at 12:38 UTC. Do not switch to unauthorized computers or bypass connection restrictions. Resume the existing native packaging task only after the authorized environment reconnects, selecting an exact stable source checkpoint and preserving older packages. Font-catalog compilation and bounded startup are required before claiming a new EXE is ready. Parent owns the promised user notification and Library delivery.

Requirements: the existing Notion Nect product-specification home is canonical, with original request IDs, acceptance conditions, source verification and delivered-version status separated. Do not replace it with another tracker or overwrite historical checkpoints. Text to Path remains a compatibility pass-through; explicit baking is a separate command. SVG already uses outlines; native .ai support is not claimed.

Approach: common Session transactions, deterministic identities, bounded geometry, one writer per scope. Focused tests and one build after actual changes; no repeated broad audits or full-suite runs. The cloud snapshot has no Git metadata; publish through expected-SHA development-branch updates and verify the remote ref.

Authority: existing Nect implementation/tests/docs/dev-branch/push/Draft PR permission applies. No protected-main merge, formal release, force/history rewrite, authentication, permissions or billing changes. Other stopped missions stay stopped.
