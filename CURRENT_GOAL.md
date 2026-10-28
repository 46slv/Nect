# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — 2026-10-28 focused development

Goal: Repair editing cancellation, duplication references and project/preset persistence; safely combine reviewed Polar/Angular work on a development branch, with a fresh Windows candidate and separate native evidence.

Current phase: Astra parent task 01a180d5-0527-71f1-9523-140ed437e3bd owns source/integration at D:/Documents/Nect-worktrees/astra-sprint-20261028. Sprint began approximately04:46UTC, closeout target06:46UTC. Existing Windows task01a180bc-2609-7762-b126-cf6ea33ae709 owns native GUI; no duplicate GUI writer. Independent implementation/test workers returned committed candidates and released source.

Proven: Start main9ea7ea5947e0cdb7f01dd7d558749cdd7144edd9. PR96 squash tree equals old Mask branch; its commits were not replayed. Polar/Angular product+tests match PR98 tree except deliberate new fixes. PR102985922a and PR103d5ddf09 changes retained. Issue100 failed on internal ref() duplication before repair; focused core51 and real Chrome Ctrl+D/Ctrl+C/V/history/reopen passed in worker. New numeric-render-race.e2e.js fails before fix when deferred render auto-commits a draft, then passes with numeric/expression drafts preserved, Escape, exact Undo/Redo and reopen. Original rich-text expectation1.35 unchanged. Deterministic pre-input render reproduction loses native number select-all, inserts135100 instead of135, rejects range and leaves height undefined; focus-to-commit reservation now passes that same case. Historical CI event trace was unavailable, so exact historical ordering remains inferred. New project-integrity-ui.e2e.js proved Feather preset save/apply and SVG asset/schema preservation via controls, actual browser Save As download and Open; Escape guard failed before fix and passes after fixing menu interception. New Polar regression caught open/closed cubic double seam crossings; bounded subdivision repair plus safe/rotated cases passes. See named test files for executable evidence.

Next task: Finish candidate verification recorded in https://github.com/46slv/Nect/pull/104. Retrieve its Windows artifact with source/tree/EXE digest and use the existing native tester, not a second GUI owner. Native inputs: desktop/tests/fixtures/project-integrity-native.json and D:/Documents/Nect-worktrees/astra-integrity-ui-20261028/desktop/test-results/native-ref-typography.json (SHA256 fa9fc637af2a1a990d03428e3a2d88fe1fbdc8cb02a7d7e51673bdda8dbccd4e); adjacent Markdown gives exact steps. PR104 holds terminal CI/native receipts and any remaining next action; read it before resuming.

Approach: One integration writer; isolated headless verification may run independently. Preserve all prior PRs/history and user projects. New native fixture is synthetic; use fresh output copies. Keep unit/DOM, browser, native GUI and physical-input evidence distinct.

Done for next: Reviewable Draft PR and remote HEAD parity, terminal required CI or named exact blocker, native result on exact new EXE (or explicit NOT_RUN), compact outstanding work with owner. Never use old Angular/Polar EXE f8fdfa65 evidence for these fixes.

State: Branch codex/nect-sprint-20261028, product revision775fa7b4a2d3df8f731b0859f945afe42f381cf2, Draft PR104. Old D:/Documents/Nect remains untouched; main9ea7ea5 is not merged. Aggregate verification exposed two real stale-render regressions in the initial numeric protection (images.e2e and editor.e2e); distinguishing native Tab selection from a draft fixed both, and their original tests plus original rich-text, deterministic numeric race/Tab selection, pair-kerning and rich-text DOM tests pass. Exact final aggregate/CI/native status and build hash are maintained in PR104 to avoid rebuilding an unchanged product for receipt-only commits. Earlier interrupted/failing runs are not final PASS evidence. A synthetic programmatic select after renderer-restored focus can still lose selection; no ordinary pointer/keyboard path was established, and this is not claimed fixed.

Authority: Current user's two-hour sprint explicitly authorizes edits, tests, new development branches, commits/non-force pushes and Draft PRs. No main merge, release, force/history rewrite, permission/auth/billing changes. This replaces the stale prior integration checkpoint and its completed PR84/86/88 next-task text, not the Mission Brief above.

Handoff: CONTINUE_CURRENT_TASK until bounded sprint closeout. Pending CI/native work must retain exact owner and next check conditions.
