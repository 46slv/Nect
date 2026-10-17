# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — 2026-10-17 14:01 UTC

Goal: Prioritize Shiro's hands-on feedback and font/path usability. The explicit October 17 full-font request broadens the historical mission; support remains staged and must not be described as universally complete.

Completed source: the 25-modifier catalog, graph alignment, seven interaction fixes, cubic font corner rounding, real static family/style selection, undoable Text → Multi Path baking, expandable Join Paths, character ink snapping and maintained equal gaps. See individual feature documents for limits.

New integrated slice: opt-in schema 25 variable TrueType assets, exact worker-prepared document banks, atomic async commands/history/open, experimental import and real axis controls, and cold-process CLI SVG export. Static documents remain schema 24 and retain their parser. Supported variable text is whole horizontal single-line simple Latin with verified one-to-one mapping. Complex shaping, RTL, vertical/wrap/path/rich-character/maintained-arrangement variable layouts and CFF2 are explicitly rejected. No main-thread fontkit fallback or CSP change.

Evidence: core/document 65/65 focused checks, UI 4/4 focused DOM checks, CLI 5/5 focused checks. One integrated production Web build passed: 94 modules, emitted local Worker 369.13 kB. Synthetic DOM plus real Node Worker tests are not browser/WebView acceptance. Native font-catalog Rust remains uncompiled; real offline Worker/CSP, IME/high-DPI and native packaging remain unverified. Worker output/cache limits are not a hard heap quota. Preserve exact bundled license declarations/notices without invented attribution.

Follow-on mixed-document correction: static Text retains full family/style/favorites/installed-font/import and range selection when a variable asset or node is present. Variable adoption remains a whole-text prepared transaction; variable range/native rows remain disabled. Four focused DOM tests pass with real Worker coverage of import, cancellation, history and preserved variable readiness; one new integrated build passes.

Named instances: actual validated fvar presets now use the existing asynchronous preview/commit/history path with exact coordinate matching and a Custom state. Twenty focused variable/document/UI/metadata checks and one production build pass. No schema/backend/CSP/native changes; browser/WebView visual acceptance remains open.

Anchor reposition: Transform now offers nine input-bounds presets and a draggable Anchor crosshair. Compensated absolute Position preserves the evaluated affine; Offset and source geometry are retained. Presets support zero scale; singular drag refuses. Locked/driven Anchor or Position refuses atomically. Twenty-seven focused core/DOM checks and one production build pass; real host acceptance remains open.

Ownership: core, UI, CLI, mixed-picker, named-instance and anchor-reposition workers have released their source scopes. Parent owns this integrated checkpoint and publication. Do not restart overlapping implementation or broad test suites.

Next concrete work: accept the frozen integrated slice in a real supported browser/WebView, then compile/package on authorized shiro-WS when connected. Check existing task and environment first. Do not bypass the previously denied cloud-browser sandbox or switch to unauthorized computers. Independent approved development can continue without treating native wait as a global stop.

Frozen deliveries: native b9fe3f64732e119b409a4761894c17f47893df40 was delivered at 10:59 UTC (Library libfile_0bf2f7e94fac81919256d212ac38fcef); browser cec110497cdf27a8a90bdf6afe282445e5cadf9e at 10:26 UTC (Library libfile_d75c0ab0205c8191af8626ec909fb4c6). Earlier packages remain unchanged. New source changes are NOT in those deliveries. Native startup/close passed, but full editing/save/IME/high-DPI acceptance is still unverified. Keep version-specific saved JSON separate.

Native update: authorized shiro-WS was offline at 13:39 UTC. Do not switch to unauthorized computers or bypass connection restrictions. Resume the existing native packaging task only after the authorized environment reconnects, selecting an exact stable source checkpoint and preserving older packages. Font-catalog compilation and bounded startup are required before claiming a new EXE is ready. Parent owns the promised user notification and Library delivery.

Requirements: the existing Notion Nect product-specification home is canonical, with original request IDs, acceptance conditions, source verification and delivered-version status separated. Do not replace it with another tracker or overwrite historical checkpoints. Text to Path remains a compatibility pass-through; explicit baking is a separate command. SVG already uses outlines; native .ai support is not claimed.

Approach: common Session transactions, deterministic identities, bounded geometry, one writer per scope. Focused tests and one build after actual changes; no repeated broad audits or full-suite runs. The cloud snapshot has no Git metadata; publish through expected-SHA development-branch updates and verify the remote ref.

Authority: existing Nect implementation/tests/docs/dev-branch/push/Draft PR permission applies. No protected-main merge, formal release, force/history rewrite, authentication, permissions or billing changes. Other stopped missions stay stopped.
