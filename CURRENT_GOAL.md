# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Happy DOM observer and full-suite triage (2026-10-19)

Goal: Remove the synthetic-DOM startup failures caused by missing MutationObserver bindings, verify the focused paths and one full `npm test` run, classify remaining failures, and save a local checkpoint.

Current phase: The 82 happy-dom test fixtures that lacked the browser global now bind the real `win.MutationObserver`. The focused deletion and ordering fixtures were aligned with the editor's existing focus, all-points browsing, and scheduled-render behavior. No application source was changed.

Proven:

- Focused serial run of deletion, ordering, Auto Handles, Bend, and Smooth DOM tests: **22 PASS**.
- `git diff --check`: **PASS**.
- One full `npm test` run completed with exit code 1. Its log contains **0 `MutationObserver` errors** and 70 `test at` failure reports. The default spec reporter did not emit a root aggregate count; the 70 reports are not directly comparable to the previous aggregate of 1,559 tests / 1,447 PASS / 108 FAIL / 4 skipped.
- The prior 108 failures were classified as 77 missing observer bindings, 3 Windows child-process failures, and 28 assertion/schema/API mismatches. The current three child-process failures are environmental path issues: one cannot find `C:\Program Files\nodejs\node.exe` (`ENOENT`); two variable-font CLI tests pass `/C:/...` and Node resolves the malformed `C:\C:\...` path (`MODULE_NOT_FOUND`). No `EPERM` or `EACCES` denial was observed.
- Remaining assertion/API failures include old schema/expectation mismatches, duplicate `evaluate` declaration, and missing test helpers `serialize` / `beginRecovery`. They were not changed because this run supplied no evidence that the observer bridge caused them.
- Full-run diagnostic log: `%TEMP%\nect-npm-test-mutationobserver.log`.

Next task: Reconcile the remaining failure reports against the prior baseline and accepted contracts; fix only a clearly evidenced regression attributable to this fixture change. Then resume the Composition lifecycle acceptance where runtime access permits.

Approach: Keep the actual happy-dom observer, preserve assertions, and do not change Windows process or permission settings. Treat the three child-process errors as environment/path failures.

Done for next: Focused and full-run evidence is recorded, the local checkpoint is committed, and the exact resulting HEAD is reported to the parent.

State: Branch `codex/nect-regression-validation-v2`; pre-checkpoint HEAD `8621a15788424413918ec6d3ae4ae201a8eade80`; test diff currently touches 82 DOM test files. No push, network retry, localhost retry, or product-source change.

Authority: Parent's delegated request to fix the observer fixture setup, run target tests and one full `npm test`, classify subprocess failures, and create a local commit. Push was not requested for this follow-up.

Handoff: CONTINUE_CURRENT_TASK
