# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Accessible Composition tablist (2026-10-19)

Goal: Replace the multi-Composition header selector with an accessible horizontal tablist. Preserve the existing Session switch path so switching Composition does not change authored project state, revision, dirty state or Undo/Redo history. Scope: UI, focused tests, related documentation, local commit and normal push to the existing development branch.

Current phase: Tablist UI and focused verification are complete; the local checkpoint commit and remote synchronization are being finalized.

Proven:

- `desktop/src/main.js` renders ARIA tabs and a tab panel. Left/Right/Home/End move focus; Enter/Space/click activate through `switchActiveComposition`. `aria-selected` and roving `tabindex` track selection and focus separately.
- `desktop/src/style.css` bounds and horizontally scrolls the tab strip, truncates long labels, styles selected tabs, and retains the existing global `button:focus-visible` outline.
- `desktop/tests/blank-artwork-dom.test.js` covers 18 tabs, long names, keyboard wraparound/manual activation, clicks, and unchanged revision, dirty state, History and project snapshot. Related DOM, contract and responsive tests: 17/17 PASS.
- `npm --prefix desktop run build`: PASS; Vite reports the existing large JavaScript chunk warning. `git diff --check`: PASS; Git reports a CRLF-to-LF normalization warning for `composition-size-design.md`.
- The existing application lifecycle fixture covers project Save/Open, recovery, browser Open, public load, legacy migration, shared fonts/images, active-Composition export sizing and preservation of inactive Compositions.
- Real browser/native visual and keyboard/scroll acceptance: NOT_RUN. The prior localhost navigation denial was not retried or bypassed.

Next task: Commit only the seven tablist source/test/design/verification files and this checkpoint, push `codex/nect-regression-validation-v2` normally, then verify `origin/refs/heads/codex/nect-regression-validation-v2` resolves to the exact local HEAD. Manual native/WebView interaction acceptance remains after synchronization.

Approach: Preserve the existing Session command and switching paths. Do not retry npm/dependency installation or create another checkout. Before this commit, local base HEAD is `86c4dc91fbf4133320f2bd214f19aacb4ab34c9b`; the branch is five commits ahead of origin. Remote HEAD was freshly read as `8621a15788424413918ec6d3ae4ae201a8eade80`.

Done for next: The scoped implementation and checkpoint are committed, the normal development-branch push completes, and exact local/remote HEAD equality is confirmed. Then the only outstanding acceptance is hands-on native/browser tablist interaction.

State: Branch `codex/nect-regression-validation-v2`; pre-commit local HEAD `86c4dc91fbf4133320f2bd214f19aacb4ab34c9b`; pre-push remote HEAD `8621a15788424413918ec6d3ae4ae201a8eade80`; seven scoped tablist files modified plus this checkpoint. The target commit includes the tablist implementation, tests, related docs and this active checkpoint.

Authority: Current user instruction authorizes this Composition tablist implementation, focused tests and documentation, a local commit, and a normal push to this development branch. Main merge, release, force push/history rewrite and credential/permission changes remain excluded.

Handoff: CONTINUE_CURRENT_TASK