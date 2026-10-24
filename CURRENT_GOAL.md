# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Project import expression identity repair

Goal: Fix Issue85 so imported, renamed node references in expressions follow the new ID, while unrelated external/dangling targets and authored syntax retain their semantics.

Current phase: Sole writer on codex/project-import-expression-refs-20261024 from the current main tree. Deterministic pre-fix regression failed on stale Circle ref; noncollision control passed. Initial focused56 and aggregate2371/0fail/5skip passed. An accepted descriptor-alias case then reproduced an out-of-scope metadata/layer rewrite; copying JSON occurrences independently closes it, with final focused57/57 passed. Final aggregate and review of that bounded repair follow.

Proven: The shared import helper remaps only copied Scalar/Vector2/Color expressions using the source layer's actual imported node IDs. Existing matcher grammar, parser/validator/schema, other source/destination state and both Session transaction boundaries are retained. No changes mixed with PR84/86/87.

Next task: Complete focused/aggregate/build/config/format checks and independent scoped review; preserve exact diff/head/tree and publish a development commit/draft PR with exact-head CI evidence or precise blockers.

Approach: Reuse the established quoted-ID matching contract. Copy validated JSON occurrences independently, preserve primitives/own keys, then add same-layer expression remapping after collision allocation; enumerate existing parameter, point and remap-edit descriptors. Test actual Project and legacy Session dispatch, evaluation, Undo/Redo, JSON reopening and unchanged non-node/cross-layer/external/unselected/dangling references.

Done for next: Proven pre-fix failure and corrected behavior, preserved controls, clean isolated branch, review/draft PR, exact remote head/tree and CI with native UI limits.

State: Source base ff8bceabc3082766a2219c922d3781dba4d2128e/tree2eee60355c3179b9a14aed2557b6f2a9d2eaaeb5 equals remote main d0d9c9265a1628983f727715bc205a44aaa5c0a6. Issue85 is open/unassigned with no duplicate matching PR at admission. Existing worktrees are preserved; no protected-main mutation.

Authority: Current delegated Issue85 maintenance instruction and standing Nect development permission cover scoped edits/tests/commit/normal dev push/draft PR and independent read-only review. No main merge, release/install, unrelated feature, updater/provider, settings/security/credentials or user-session changes.

Handoff: CONTINUE_CURRENT_TASK through checks/review/draft/CI. Native UI/OS dialogs/physical rendering and actual filesystem save/reopen remain NOT_RUN; JSON roundtrip and Session tests are separate evidence. See desktop/docs/project-import-expression-refs.md.
