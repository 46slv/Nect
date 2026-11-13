# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: the current user resumed development, prioritizing practical panels and color. Edits, tests, coherent commits, non-force development pushes and normal integration after applicable checks and review are authorized. Release, force/history rewrite, credential or billing changes and bypass of safety denials are excluded. The 2026-11-12 instruction removes backward compatibility with old versions as a goal: do not work solely to preserve old formats or APIs. Protect current authored data, IDs, History and current product defaults. Product decisions remain in [Confirmed requirements](https://app.notion.com/p/01b2ff7802fc410ab4cdb8fd26264495); [REQ-211](https://app.notion.com/p/3fbfd279a6f381a9a7c9c36f5089fba1) governs color and the current [Workspace action](https://app.notion.com/p/40ffd279a6f38132856ce92091df60ea) governs the approved staged panel work. Research plans are not adopted product presets; B remains trial-only when explicitly selected.

Coarse route: panels and color → remaining adopted Session, document, shape and text requirements chosen from live dependencies → measured native stabilization and Shiro review.

## ACTIVE CHECKPOINT — Node Foundation migration

Goal: replace remaining legacy built-in node declarations and evaluator dispatch with trusted Node Foundation bindings while preserving current authored documents, stable IDs, Session commands, Undo/Redo, pixels and product defaults. This task works alone; earlier tasks remain stopped.

Current phase: as observed on 2026-11-13, all 70 current node types are registered in code commit `55993f0e7f10c0eb4187d1a51d27b46339c74e68`. PR #323 contains Boolean, Transform and Text together with the measured Boolean query-performance repair. Earlier candidates #301, #303–#320 were integrated; final #323 integration is not assumed by this record.

Proven: pre/post definition, catalog and representative runtime/output comparisons matched. The 1,024-input Boolean active-query sample returned from roughly 3.17 ms to 0.04 ms after repair, versus about 0.05 ms before migration. Focused tests, build and production-CSP Chrome flows passed, including Text / Boolean / Join / Transform / Repeater / Fill / Output edits, Undo/Redo, project save/reopen and matching canvas/SVG/PNG pixels. Four optional tests using absent local DejaVu styles were skipped. Individual evidence, exact heads and CI status are in PRs #301–#323 and checkpoint branch `codex/mission-node-migration-20261113`.

Next task: verify PR #323 exact-head CI, live main, GitHub mergeability and the prospective integration tree; merge the combined candidate only after all checks pass, then read back main and the #321/#322 statuses. If that has already occurred, verify the recorded PR and commit before selecting further work. Do not restart earlier tasks.

Approach: one writer in the task-owned D: worktree; retain current Session/evaluator and production-renderer contracts. Native WebView2, native dialogs, physical IME/input and native OS-font discovery remain untested by this checkpoint.

Done for next: successful required checks, matching prospective and actual main trees, normal non-force integration and remote readback. A local test or PR alone is not main integration.

State: last observed main before final #323 integration was `a38141effca2db5f834cb9cf4b26098533bfea4f`, tree `6972841ac585ad6884ebeae24677080f8326824b`. The final code and checkpoint travel in PR #323; use its current head and merge state as the live pointer. Other shared worktrees and the unrelated running development server are not owned by this task. The earlier stopped checkpoint remains `codex/mission-checkpoint-20261112` / `0f777b890646c99d91e2104ab2df778b86878ba5`.

Authority: the user's 2026-11-13 instruction authorizes this task alone to perform scoped node migration and ordinary validated Git integration. Preserve the earlier automatic-review refusal for #296 and the Field denial; do not retry either through another route. Release, added billing, force/shared-history rewrite and unrelated data changes remain outside scope.

Handoff: CONTINUE_CURRENT_TASK.