# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - SVG preserveAspectRatio meet alignment (2026-10-19)

Goal: Extend the root `viewBox` Transform to preserve all nine `meet` alignment choices while retaining prior default and `none` mapping, physical viewport sizing, and the separate Import/Add SVG contracts.

Current phase: The alignment matrix, focused tests, documentation, and production build are complete and synchronized to the development branch.

Proven:

- The prior SVG viewport matrix, 96 CSS px/in physical root dimensions, paint grouping, and single-transform Add SVG path are present at branch base `62563a9deb5c4054278c61162a6f02ad483ab512`.
- Focused Import SVG, Add SVG, and SVG export-unit suites: 26/26 PASS. Independent expected matrices cover all nine min/mid/max X/Y alignments with separate horizontal and vertical slack, non-zero origins, default/explicit mid alignment, and unchanged `none` behavior.
- A physical 100 x 50 mm Add SVG fixture verifies `xMaxYMin meet`, exactly one viewport Transform, unchanged destination dimensions, serialization, one-step Undo, and Redo.
- `slice`, `defer`, invalid alignment values, and clipping remain unsupported and fail closed. Production build, scoped formatting, and `git diff --check`: PASS. Full `npm test` and actual WebView2 import/export remain NOT_RUN.
- Implementation commit `1d130fbd61c2d7583b922ed57a48e33bc9b2df28` was pushed normally and matched the origin branch SHA.

Next task: The Mission Owner should select the next eligible Route obligation from current evidence; this alignment checkpoint is complete.

Approach: Normalize supported preserveAspectRatio tokens; compute X/Y meet offsets independently from Min/Mid/Max alignment while keeping uniform meet scale. Retain the paint-mode Transform so stroke and gradient behavior stays shared with the prior viewport mapping.

Done for next: All nine meet alignment matrices match independent expected values; default and `none` remain unchanged; Add SVG applies one mapping and preserves destination size, serialization, Undo, and Redo; unsupported slice/clip behavior remains explicit; focused tests, formatting, build, commit, and remote evidence are recorded.

State: Branch `codex/nect-regression-validation-v2`; implementation commit `1d130fbd61c2d7583b922ed57a48e33bc9b2df28` is pushed and matched the remote SHA. The follow-up checkpoint closeout is committed and pushed on this branch; its terminal branch SHA was verified against origin. Preserve the pre-existing untracked `desktop/native-artifact-4edbda9/` outputs untouched.

Authority: The user's follow-up authorizes this bounded meet-alignment implementation, focused tests/build/docs, commit, and normal development-branch push. Keep `slice`/clip unsupported. Do not re-audit the Completion Route, force push, mainline merge, release, dependency/network retry, credentials/permission changes, or alter the preserved trial outputs.

Handoff: CONTINUE_CURRENT_TASK
