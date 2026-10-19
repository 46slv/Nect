# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Main data-URL test loader (2026-10-19)

Goal: Make synthetic-DOM tests that import `src/main.js` through a data URL resolve every remaining same-root relative module, and keep the Artboard Pan regression test exercising actual artwork.

Current phase: A shared test helper resolves static `from './…'` and dynamic `import('./…')` specifiers against `desktop/src/main.js`. It is used in the 37 data-URL test files that had not resolved all relative imports. The Artboard Pan fixture now loads a real Circle through `window.nect.load()` and bridges `MutationObserver` into the synthetic browser globals.

Proven:
- `node --test tests/main-data-url-loader.test.js tests/artboard-pan-dom.test.js`: **2 PASS**.
- `node --check` on the helper, its test and Artboard Pan test: **PASS**.
- Prettier check for the three focused files: **PASS**.
- One full `npm test` run: **1,559 tests, 1,447 PASS, 108 FAIL, 4 skipped**. Of the failures, 77 synthetic-DOM files lack a global `MutationObserver` binding; 3 subprocess cases fail to launch the configured Windows Node path; 28 remaining cases report assertion/schema/API mismatches, an `evaluate` duplicate declaration, or missing `beginRecovery`. Those remaining cases are outside this loader change; without a baseline run they are recorded as unresolved test/code contract failures, not attributed to this patch.
- The full run includes the Artboard Pan regression and it passed. No production code was changed by this checkpoint.
- The previously recorded localhost navigation denial remains a browser acceptance blocker; it was not retried.

Next task: Triage the full-suite test environment (`MutationObserver` bridge and Windows subprocess path), then investigate the 28 assertion/API contract failures against current requirements. Continue the Composition lifecycle acceptance when the relevant runtime is available.

Approach: Keep the import helper confined to test setup. Classify fresh full-suite outcomes by test harness, host runtime and app contract; do not turn a passing synthetic test into a browser acceptance claim.

Done for next: Each remaining full-suite failure is either fixed with a focused regression or durably attributed to an explicit environment blocker; composition Open/Save/recovery/public-load and browser/native lifecycle evidence is current.

State: Branch `codex/nect-regression-validation-v2`; pre-checkpoint HEAD `d5c34d59185655c7a44513c91bf9cff48310adcc`; these loader and test changes are the pending checkpoint commit. Working tree is expected clean after that commit. Build was not required for this test-only change. Full `npm test` ran exactly once after the helper update.

Authority: Current delegated task authorizes the shared test-loader fix, focused verification, one full `npm test` run and a development-branch checkpoint. No dependency install or network retry, restoration clone, backup or upload was performed; localhost denial was not bypassed.

Handoff: CONTINUE_CURRENT_TASK
