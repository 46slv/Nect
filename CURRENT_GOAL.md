# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Multi-input Boolean (2026-10-19)

Goal: Extend the existing Boolean node to support ordered 3+ Path inputs, preserve the existing binary behavior and old A/B files, and expose the same operation through UI and command Session.

Current phase: Implementation, focused verification, documentation, and production build are complete; local commit is pending.

Proven:
- `desktop/src/core.js` evaluates ordered Boolean inputs as editable Path output. Subtract is A minus the union of later inputs; Difference retains the current symmetric-difference operation.
- Legacy A/B JSON stays at its prior schema until the virtual `input_pending` socket is used or locked. Expanded input keys are stable and schema 40 requires a canonical empty spare.
- The UI renders and wires the virtual socket through the shared `connect` command. Source navigation traverses every persisted Boolean input and ignores empty additional spares.
- Focused core, UI, Join, and source-navigation tests: 57/57 PASS. JavaScript syntax, selected Prettier checks, `git diff --check`, and `npm run build`: PASS.
- Native/WebView visual acceptance: NOT_RUN; synthetic DOM covers wiring, spare labels and Undo/Redo.

Next task: Create a local commit from the verified Boolean implementation and checkpoint record; report its SHA and the remaining native acceptance status.

Approach: Re-read the scoped diff and status, commit only the Boolean source, tests, architecture/requirements/verification notes, and this checkpoint. Keep this checkpoint local; no push or remote mutation.

Done for next: The local commit contains the coherent implementation and passing focused evidence. The remote branch remains unchanged by this checkpoint.

State: Branch `codex/nect-regression-validation-v2`; base HEAD `11d2aa77d0fd2c4968ae02301cf366125eb33804`; scoped Boolean source, tests, docs, verification, and checkpoint changes are in the working tree. Build output is ignored. No remote operation has been performed for this checkpoint.

Authority: The current user instruction authorizes the scoped Boolean implementation, related tests and documentation, production build, and local commit. This checkpoint does not authorize a push, merge, release, or history rewrite.

Handoff: CONTINUE_CURRENT_TASK
