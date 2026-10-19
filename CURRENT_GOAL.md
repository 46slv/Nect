# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Physical-unit SVG import/export boundaries (2026-10-19)

Goal: Preserve physical SVG root dimensions in the 96 CSS px/in document basis and prevent unsupported viewBox transforms from silently changing size or position.

Current phase: Bounded Import SVG and Add SVG tests, importer repair, documentation, and production build are complete; checkpoint commit and normal branch synchronization remain.

Proven:

- Import SVG converts absolute root lengths px/mm/cm/in/pt/pc/q to CSS pixels. A 100 x 50 mm source round-trips through fixed mm and cm SVG export; a matching zero-origin viewBox and explicit `xMidYMid meet` retain dimensions.
- Non-zero viewBox origins and root/viewBox scale or letterbox mismatches are rejected because transform-preserving geometry import is outside the current subset. Add SVG appends px user-space geometry without resizing the destination Composition and rejects an invalid source atomically.
- Focused Import SVG, Add SVG, and root-unit tests: 20/20 PASS. Scoped Prettier and `git diff --check`: PASS. Production build: PASS (136 modules; existing large-chunk warning).
- Full `npm test` and actual WebView2 file interaction: NOT_RUN. No dependency installation or network retry occurred.

Next task: Evaluate whether transform-preserving import for non-zero origins and mismatched root/viewBox sizes should be the next bounded compatibility increment; the current verified behavior remains explicit rejection.

Approach: Keep Import SVG's replace-document contract distinct from Add SVG's append-to-Composition contract. Extend transforms only with fixtures that prove 96 CSS px/in geometry and expected preserveAspectRatio mapping; otherwise keep rejecting unsupported cases.

Done for next: Physical root lengths and the safe zero-origin identity mapping are verified; unsupported viewport transforms cannot silently alter imported artwork; the source/tests/docs checkpoint is committed and synchronized.

State: Branch `codex/nect-regression-validation-v2`; pre-commit HEAD `3dba12f22800861cf7bdc2f325ea7cf5b68bf84c`. Scoped source/tests/docs are modified. Preserve pre-existing untracked `desktop/native-artifact-4edbda9/` outputs. Remote was at the pre-commit HEAD at start.

Authority: The user's current delegated instruction authorizes this bounded SVG import/export review, corrective implementation, focused tests/build, documentation, commit, and normal development-branch push. Do not re-audit the Completion Route, force push, mainline merge, release, dependency/network retry, credential/permission change, or alter the preserved trial outputs.

Handoff: CONTINUE_CURRENT_TASK
