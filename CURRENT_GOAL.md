# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Combined development test candidate (2026-10-20)

Goal: Integrate the completed PR35 Layer-ordering, PR36 dither-mask and PR37 graph-autopan repairs at exact baseline acca2616 in a new isolated development branch. Preserve original branches and prepare a reviewable source/build handoff; Windows artifact remains undistributed until actually built, measured and saved.

Current phase: All three source repairs and PR37 documentation follow-up are integrated on the new development branch; combined63/63 focused tests and production build137 modules PASS. Draft PR38 is open. Source-union checks preserve parent product files/functions exactly. Native artifact is NOT_BUILT / NOT_DISTRIBUTED.

Proven: Source packets are PR35 product cae664a60ca827d90583d2e1bedad178bc9870a6 / terminal646f80b0b116f0a8bc52f1f9a10dc081e9ea3458 (Layer name dragging, visual insertion and shared Session history); PR36 product efb0160b03996891c97b5738133f7fb5f0732ea2 / terminalfb36fb8d2c395f7017447fe7cb76412559a038b3 (finite larger mask preparation and exact zero-length derived-segment handling, with both Image/Shape retention requirement); PR37 ec474b142296a67c23519389baa904c2aa156a79 (skip graph preview/wire work on frames with no actual scroll). Original evidence and residuals remain in desktop/docs/layer-panel-interaction.md, dither-input-budget.md, dither-controls.md and graph-autopan-work.md. Combined source tests63/63 PASS. Production build137 modules, main syntax, scoped format and diff checks PASS. Separate known-baseline probes retain three schema/fingerprint failures with matching acca2616 values and overview0vs6 reproduced on both base/integration. Full suite and UI/native/frame-time acceptance NOT_RUN. PR37 documentation ada9ac547a8d6d0e0a9e9fb4d667a443c5fe5309 is included exactly; no product inputs changed after tested28a5b9cdb872309e62e76d5032add974afbdf9d8.

Next task: Parent routes the pinned candidate source6c6c0a5427dbc3bc59ea206bffdb232a195d41f5 to an already provisioned Windows build host. Follow desktop/docs/integration-candidate-20261020.md, preserve original EXE, and bind actual generated artifact/source/hash before delivery and targeted human retest. No unchanged failed native build is retried here.

Approach: Merge only these completed branch snapshots into this task-owned D: worktree. No new feature, Image-output implementation, UI/native operation, protected-main merge, original-branch update or history rewrite. Keep source and distributed executable identities separate.

Done for next: Three source repairs and their evidence preserved, focused integration and frontend build verified, development checkpoint synchronized/Draft PR reviewable, exact Windows artifact limitation and preparation/verification steps documented. A source build is not native acceptance or distribution.

State: codex/nect-test-candidate-20261020, baseline acca2616ab38c703bcf3e4b0ef465eeae7c62ffc, D:/Documents/Nect-worktrees/codex-task5-integration-20261020. Sole integration writer assigned by parent. Source and graph documentation writers completed. All supplied terminal refs are ancestors; Draft https://github.com/46slv/Nect/pull/38 targets codex/nect-regression-validation-v2. Current source instructions/branch refs freshly read. Build/cache/temp use task-owned D: paths; no capacity cleanup/deletion or environment installation is authorized.

Authority: Current user integration delegation plus existing Nect code/test/docs/commit/development push/Draft PR authorization. Parent owns Notion and actual delivery. Known Rust toolchain/SDK26100 absence and unresolved MSVC are retained from existing evidence; do not retry unchanged Windows build/inventory, install tools, alter auth, delete for space, or operate UI. No new EXE exists.

Handoff: LOCAL_WAIT for provisioned Windows build/artifact measurement/native retest. Combined source is reviewable in Draft PR38; parent owns delivery/Notion. Broader Nect Mission and Image-output dither remain open.
