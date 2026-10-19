# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - PNG PPI density metadata (2026-10-19)

Goal: Keep PNG raster dimensions and physical density metadata consistent when the existing export UI is set to a selected PPI, without changing raster samples or scale-mode behavior.

Current phase: Implementation, focused verification, documentation, and production build are complete; checkpoint commit and branch synchronization are the closing actions.

Proven:
- The Desktop exporter already offered mutually exclusive scale and physical-PPI modes. PPI raster dimensions use the CSS baseline `PPI / 96`; the PPI selector was pre-existing and was not reimplemented here.
- PPI-mode export now inserts or replaces exactly one CRC-valid `pHYs` chunk before image data, with equal rounded pixels-per-metre values. 300 PPI is stored as 11811 px/m (about 299.9994 PPI). Scale mode returns the canvas PNG unchanged; all non-density chunks are preserved.
- Focused PNG metadata and sizing tests: 9/9 PASS. JavaScript syntax, scoped Prettier check for `raster.js` and `png-density.test.js`, `git diff --check`, and `npm run build`: PASS.
- Actual WebView2/canvas file inspection and device-pixel-ratio independence: NOT_RUN. The synthetic canvas adapter test verifies which PNG receives metadata.
- The preceding multi-input Boolean checkpoint `7376af45fbee380ff8ba0842c3d2af081e0671ee` was normally pushed and its remote branch HEAD was verified equal before this checkpoint.

Next task: Inspect an actual PNG written by the current WebView2 export route and confirm its `pHYs` value and raster dimensions at selected PPI; then continue the next small export acceptance item from the crosswalk.

Approach: Use only the existing local runtime and focused tests. Treat actual-file and device-pixel-ratio checks as separate acceptance evidence. Do not install dependencies, retry network access, bypass browser restrictions, or run the full suite.

Done for next: The pHYs contract is covered by chunk-level and export-adapter tests, scale mode and raster bytes stay unchanged, and the implementation checkpoint is committed and synchronized on the development branch.

State: Branch `codex/nect-regression-validation-v2`, based on synchronized HEAD `7376af45fbee380ff8ba0842c3d2af081e0671ee` (see Git history for the implementation checkpoint tip). The source, focused test, crosswalk, verification record, and this checkpoint form one successor change. Generated build output is ignored; verify current local/remote HEAD and cleanliness before resuming.

Authority: The user's current follow-up explicitly authorizes this bounded PNG PPI / pHYs implementation, associated limited tests and records, local checkpoint commit, and ordinary push of the development branch. Force push, merge, release, credentials, permission changes, dependency install, network retry, and Notion writes remain excluded.

Handoff: CONTINUE_CURRENT_TASK
