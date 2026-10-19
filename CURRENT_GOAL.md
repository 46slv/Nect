# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Format-scoped export settings presets (2026-10-19)

Goal: Save and reapply supported SVG and PNG export settings in the toolbar, keeping each preset format-specific and separate from project data and Undo.

Current phase: Implementation, focused verification, source commit, and normal branch synchronization are complete.

Proven:

- SVG presets contain only fixed/responsive sizing, supported units, and geometry precision. PNG presets contain either one supported scale or positive PPI. Exact-key validation rejects mixed-format and unsupported fields.
- The toolbar saves, reloads, and applies settings through the existing export controls. Applying one format leaves the other format's settings unchanged. Presets use a versioned local preference key and do not change project JSON or Undo history. Invalid saves surface feedback through the existing status area.
- Focused preset, DOM integration, responsive SVG, unit, PNG sizing, and PNG density tests: 23/23 PASS. Scoped Prettier, `git diff --check`, and production build: PASS (136 modules; existing large-chunk warning).
- Full test suite and actual WebView2 interaction: NOT_RUN. No dependency installation or network retry was made.

Next task: Re-read the live Composition/session completion route and choose its next eligible checkpoint, preserving the existing Goal and human gates.

Approach: Use current repository/route evidence to select one bounded implementation or validation step. Keep the next checkpoint focused and continue only within the existing authorization.

Done for next: Select the next mission checkpoint from current requirements and route evidence; this export preset checkpoint remains implemented, verified, and synchronized.

State: Feature source commit `ca6d206481cd5371febdbc51b5102ae6ff8e4224` is pushed on `codex/nect-regression-validation-v2`; remote branch was verified at the same SHA. The only pre-existing local untracked files are the two outputs under `desktop/native-artifact-4edbda9/`.

Authority: The user's current delegated instruction authorizes this bounded export preset work, focused checks, records, commit, and normal development-branch push. Do not perform a force push, mainline merge, release, dependency/network retry, credential or permission change, or modify the preserved trial outputs.

Handoff: CONTINUE_CURRENT_TASK
