# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Manual update review gate

Goal: Share the existing preview-check orchestration with the Build interface and establish an explicit candidate-review/confirmation boundary while production remains unconfigured.

Current phase: Manual review gate implementation and local verification complete. Recovery owner 01a176cf-8ae4-7dd2-86bc-7f2f090519c7 completed prior owner 01a16f70's released draft in the same isolated checkout. Production retains literal provider=null and supplies no source/preparation adapter.

Proven: Focused controller/fixture/discovery/Build/version tests51/51 pass after the refresh re-entry repair. Six callback/action cases first failed and then passed: clock/readContext triggering dispose, cancel or a new check. refresh now preserves terminal state, generation and review identity after validation. Three regressions reproduced synchronous cancellation, disposal re-entry and late validation overwriting cancellation; all pass after generation/terminal guards. Prior d878d8d3 frontend aggregate2398 PASS/0 FAIL/5 SKIP (2403 total), full formatting, native-build configuration4/4 and Vite build pass. The initial aggregate2396 PASS/1 FAIL/5 SKIP failed only because the bundle lacked historical49283bca; importing canonical history resolved it (angled-grid7/7 and final aggregate PASS). Existing native05102869 acceptance belongs to a different, older candidate. New updater native UI remains NOT_RUN.

Next task: Review the development PR and its exact-head CI. The bounded manual gate is complete locally; live update delivery requires separate authority and the dependencies below. Exact publication head/result is recorded in the owner checkpoint and recovery RESULTS.md. No main integration in this scope.

Approach: Reuse discovery/version/audience/expiry validation and literal rendering. Frozen review context/request/response, single-use confirmation, new-request retry, cancellation/disposal and late completion handling. Preparation remains injected test code only.

Done for next: Local focused checks completed for the refresh repair; aggregate/frontend/config evidence remains bound to prior d878d8d3 until new-head CI completes, no production updater side effects, and canonical-parent development candidate preserved. Remote CI and native UI must retain their own observed status.

State: Original bundle HEAD453cb1956bbd6e73509127f14293aed0e1d55816 and canonical main6652b91cca5c138c4c929b689a9b484db4bfd52c share tree62d4923e6e489c30b420c5f0f86b43288bb52ca3. Preserve the local candidate on codex/manual-update-review-gate and publish an identical-tree commit with canonical main as parent on codex/manual-update-review-gate-recovery. No history rewrite, synthetic ancestry push or other-worktree mutation.

Authority: Current recovery scope and existing development permissions cover local implementation/tests, commit and development PR. No production provider/feed/network/download/install/restart, native updater command, dependency/security changes, main merge or release.

Handoff: CONTINUE_CURRENT_TASK. Live delivery requires native-authoritative preview version/channel policy, durable trusted distribution/signing/package/bootstrap and a separately approved install/save/rollback lifecycle. See desktop/docs/preview-update-check.md.
