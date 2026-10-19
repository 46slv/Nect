# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Responsive Composition header (2026-10-19)

Goal: Keep the Composition and history controls reachable when the toolbar wraps at 900×620. Undo/Redo must remain available to normal pointer and keyboard input without changing project-session semantics.

Current phase: The header now grows with wrapped rows and the app grid places the workspace after it. Code commit 9062720f1025e1e22438c0830a829e261f80f5c9 is complete. Real browser confirmation remains pending because local navigation was denied.

Proven:
- The prior CSS had flex-wrap: wrap with a fixed 48px header; the workspace began at that 48px edge, so later header rows overlapped the Tools nav and Playwright reported it intercepted the Undo click.
- The new structural layout regression test PASS: node --test tests/responsive-header-layout.test.js.
- JavaScript syntax checks PASS; the new test file passes Prettier.
- npm run build PASS, with the existing large JavaScript chunk warning.
- The existing 900×620 browser E2E now covers normal Undo click, keyboard Redo/Undo and normal Redo click, but was not rerun after the fix because localhost navigation returned ERR_NETWORK_ACCESS_DENIED. No alternate host, route or settings were tried.
- Aggregate npm test remains FAIL for a separate loader defect. The narrow reproduced failure is tests/artboard-pan-dom.test.js: it imports main.js through a data URL after rewriting selected imports but leaves the first ./text-wrap.js specifier relative; Node fails resolving that specifier from the data URL. No broad test-harness rewrite was made.

Next task: Run the existing 900×620 E2E in an allowed browser environment and confirm Undo/Redo by normal click and keyboard. Then continue the still-open browser/native file lifecycle acceptance.
Approach: Use the existing E2E viewport and ordinary input only. Do not force-click, change viewport to hide the issue, or bypass the localhost access denial. Keep the unrelated data-URL loader fix as a separate, narrowly scoped task.
Done for next: Real 900×620 browser evidence shows the wrapped header and workspace do not overlap and all Undo/Redo inputs work; otherwise record the exact remaining failure.
State: Branch codex/nect-regression-validation-v2; responsive fix commit 9062720f1025e1e22438c0830a829e261f80f5c9; no push. Documentation follow-up is being recorded after this implementation commit.
Authority: Current user delegation authorizes this responsive fix, its regression coverage, focused checks and local commits. Browser access denial must not be bypassed; no network retry, settings change, push, merge, release or force operation.
Handoff: CONTINUE_CURRENT_TASK
