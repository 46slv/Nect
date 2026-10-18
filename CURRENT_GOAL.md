# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — REQ-23 named node preset first phase (2026-10-19)

Goal: Document-local named snapshots of self-contained selected source/effect
nodes, additive cross-Composition apply with fresh IDs, ordinary node re-editing,
explicit update, persistence and one Undo per changed operation.
Current phase: implementation and focused verification complete; source pushed,
Draft PR32 open against codex/req31-handle-polar-20261019.
Remote-verified implementation: 88c4b257df92c4bbe9199e39697be96a6781f1f3.
Draft PR: https://github.com/46slv/Nect/pull/32.
Proven: 35 focused tests and production build PASS; actual Note Chrome 155
fixture PASS for save/apply/edit/update, Undo/Redo, persistence and atomic refusal.
Evidence: desktop/docs/named-presets.md; desktop/tests/named-presets.test.js and
named-presets.e2e.js; portable Chrome receipt under desktop/docs/evidence.
Next task: Review the bounded Draft PR at its exact remote SHA.
Approach: sole source writer in independent task-6/Nect-req23, branch
codex/req23-named-presets-20261019, base ca376c67529364a4518bdf06b6893bd3c8896392.
Done for next: preserve partial REQ-23 scope and native/physical-IME gaps.
State: schema39 optional nodePresets; existing version floors/backfills preserved.
Admission rejects Output/Composition Reference, external node dependencies and
missing/changed document assets. Apply preserves existing Output and connections.
Authority: current delegation permits code/test/docs/dev commit/push/Draft PR;
mainline merge, release/distribution and auth/permission changes excluded.
Native Tauri/EXE and physical IME remain unverified. No global full-suite audit,
unrelated repairs, old worktree/user-artwork edits or Notion writes.
Macro public controls, external preset files/shared libraries and effect-stack
redesign are excluded. This is bounded partial REQ-23 coverage.
Handoff: CONTINUE_CURRENT_TASK.
