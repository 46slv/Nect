# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - SVG viewBox viewport mapping (2026-10-19)

Goal: Map supported SVG `viewBox` user coordinates into the physical root viewport without losing non-zero origins, scale, stroke, or user-space gradient placement. Keep Import SVG replacement and Add SVG append semantics separate.

Current phase: The bounded viewBox mapping is implemented, verified, documented, committed, and synchronized to the development branch.

Proven:

- Absolute root lengths still convert px/mm/cm/in/pt/pc/q to the 96 CSS px/in basis; root artboard width and height remain physical viewport dimensions.
- Focused Import SVG, Add SVG, and SVG export-unit suites: 25/25 PASS. Independent matrices cover non-zero origin, a 100 x 50 mm viewport, default `xMidYMid meet` letterboxing, and `none`; percentage geometry uses viewBox user-space dimensions. Paint group fixtures retain stroke and gradient markup. Add SVG retains one viewport Transform and leaves the destination size unchanged.
- Other alignment modes and `slice` fail explicitly. `<clipPath>` and `clip-path` remain unsupported and fail closed.
- Scoped Prettier and `git diff --check`: PASS. Production build: PASS (136 modules; existing 1.34 MB chunk warning). Full `npm test` and actual WebView2 import/export remain NOT_RUN.
- Implementation commit `1d1e781361885636dde3eaa82bd19485384007de` was pushed normally; `git ls-remote` confirmed the same origin branch SHA.

Next task: The Mission Owner should select the next eligible Route obligation from current evidence; this bounded viewBox checkpoint is complete.

Approach: Apply the viewport affine mapping once after paints so strokes and user-space gradients follow the same SVG coordinate transform. Copy that single mapping to the final output of the Add SVG source Composition while leaving source geometry in user units.

Done for next: Independent expected-matrix tests pass for non-zero origins, physical root sizing, meet letterboxing and `none`; Import and Add paths pass focused assertions for paint mapping and atomic unsupported input; build, commit, and remote synchronization evidence are recorded.

State: Branch `codex/nect-regression-validation-v2`; implementation commit `1d1e781361885636dde3eaa82bd19485384007de` is pushed and was confirmed as the matching remote SHA. The follow-up closeout record is committed and pushed on this same branch; the terminal branch SHA was verified against origin at task closeout. Preserve the pre-existing untracked `desktop/native-artifact-4edbda9/` outputs untouched.

Authority: The user's delegated instruction authorizes this bounded SVG viewBox implementation, tests/build/docs, commit, and normal development-branch push. Do not re-audit the Completion Route, force push, mainline merge, release, dependency/network retry, credentials/permission changes, or alter the preserved trial outputs.

Handoff: CONTINUE_CURRENT_TASK
