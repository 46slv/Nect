# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Shared dither source mask input repair (2026-10-20)

Goal: Repair the reported 256-point Luminance Mask failure for both dither nodes without removing computation/output safeguards or changing authored Text/path structure. Actual user file/build are unknown.

Current phase: Implementation and focused verification complete in the isolated dither branch; synchronize code and reviewable Draft PR. Native build remains the known environment wait and is not retried.

Proven: Both Ordered Dither and Error Diffusion Dither reproduce the exact 256-point failure using bundled-font live Text at acca2616. Raising input limits exposed exact zero-length font cubics hitting subdivision depth; derived-mask-only skipping preserves source records and holes. Input preparation now has finite 10000 points/256 contours/32768 flattened vertices/131072 subdivision nodes. Existing depth24, precision, 2000000 pair-work,4096 grid,100000 generated/output and10000 output contour caps remain. New/mask16/16 PASS; shared consumers60/63 PASS, three matching baseline schema/fingerprint failures independently reproduced at acca2616. Tests retain deterministic output, native codec, SVG, history and atomic refusal. Details/log boundaries: desktop/docs/dither-input-budget.md and desktop/test-results/dither-input-budget/.

Next task: Commit/non-force push and create Draft PR against codex/nect-regression-validation-v2. Parent integrates reviewed fixes, selects a provisioned Windows build host and binds a distinct artifact/source/hash before human retest. No user UI/native acceptance is claimed here.

Approach: Modify only common mask preparation limits, exact redundant-segment handling and actionable budget errors. Keep source evaluation pure and all consumers on the same helper. No automatic Bake/coarsening, algorithm redesign, format, sampling, color-system or UI redesign.

Done for next: Focused checks/build/format coherent, code and evidence committed/pushed, Draft PR and remote SHA verified, exact native/artifact limitation reported. Three baseline failures remain visible and unrelated scopes untouched.

State: codex/nect-dither-input-budget-20261020 at base acca2616ab38c703bcf3e4b0ef465eeae7c62ffc; isolated D:/Documents/Nect-worktrees/codex-task5-dither-budget-20261020. PR35 Layer reorder branch stays646f80b and is not included or modified. Sole source writer assigned by current parent delegation. C: full; task dependency/temp cache on D:. Native toolchain/SDK26100 unavailable and MSVC unresolved, known from preceding task; no repeated build/tool inventory or UI operations.

Authority: Current user's finite dither bug request and authorized Nect code/test/docs/commit/development push/Draft PR scope. Parent owns Notion and user delivery. No protected-main merge, release, force/history rewrite, credentials/security changes, denied browser retry or interaction/status question to the separate local tester.

Handoff: CONTINUE_CURRENT_TASK through source synchronization. Source repair and distributed EXE are separate; broader Mission remains open.
