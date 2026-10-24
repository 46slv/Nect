# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Approved offline integration

Goal: Integrate the already implemented Build commit display, offline preview discovery fixtures and imported expression-reference repair while preserving the merged joystick viewport behavior.

Current phase: One source writer on the isolated codex/integration-84-86-88-20261024 branch on SHIRO-WS. The reviewed candidates follow the approved PR84, PR86 and PR88 sequence; live PR state determines which unmerged stage remains. The native UI task remains separate and must not be duplicated or interrupted.

Proven: PR87 is merged as main05102869b66c814af16db22e626c15bc217de306/tree5e2219a439457b694a124008874657b34a9692fa, with exact-main Core/Windows CI and actual Chrome directional checks passed. PR84 and PR86 initial source reviews passed; PR84 integration treeb8cd5868827c1553cff166579f2d2b17583eb47f preserves all PR87 files. Earlier PR CI remains historical until the current integration heads pass their required gates.

Next task: Complete exact-candidate review and focused checks, publish ordinary merge commits to the existing development branches, confirm required CI at each new head, then integrate any still-unmerged PR84, PR86 and PR88 in that order. Verify the final Windows artifact against its actual commit/tree/run and external archive digest.

Approach: Reuse existing pure discovery/version/identity validators and literal rendering. Keep the fixture client outside the production import graph, with injected source/request/clock and bounded expiry. Resolve checkpoint-only conflicts without altering unrelated product behavior.

Done for next: Reviewed exact trees and terminal required checks for each authorized normal main merge; final archive/receipt/EXE identity, accurate remaining native acceptance limits and released source ownership.

State: Other source worktrees and PRs are preserved. PR86 product files are unchanged from its reviewed candidate; only this checkpoint combines the completed joystick evidence with current authorized integration work.

Authority: The owner's2026-10-24T17:16UTC instruction approves GitHub/publication work for these already requested tasks, including Ready-for-review transitions and normal main integration. No force/history rewrite, provider/feed activation, updater download/install/apply, keys, credentials, security/settings change, app replacement, restart or user-session operation.

Handoff: CONTINUE_CURRENT_TASK. Native UI remains a separate acceptance row; no browser/unit/compile result is native acceptance. Live update distribution and safe apply/rollback remain outside this offline increment. See desktop/docs/preview-update-check.md.
