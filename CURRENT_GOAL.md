# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT

Goal: Complete independently actionable editing consistency fixes and keep the adopted requirements executable while Shiro reviews the delivered trial.
Current phase: dot-side implementation, reopened by Shiro on 2026-10-17 09:16 JST. Human trial review is a local acceptance dependency, not a stop for independent code or Brief work.
Proven: Trial 7496aee0a9437a6f91eef72439a414a495840d31 delivered privately with EXE, README and license. Web build, Rust 21/21, offline Windows release and targeted browser flows passed. The earlier 703/709 aggregate at 768 MB had six OOM files; those passed individually at default heap (31/31). Do not call this a final aggregate pass. Native GUI, physical IME, Skew visual parity and Alt-wheel hands-on review remain open.
Next task: Preserve the numeric preference lock fix (a9ecc62) and Alt-wheel regression (411303c). Repair hover-only Remap handles that remain visible after leaving the canvas; preserve selected-point handles. Resume the bounded Windows/browser verification only when its safe checkout is available. Preserve already verified evidence rather than rerunning all checks.
Approach: Existing common Session, same stable identities and shared affine. Root dot owns source; bounded Luna read-only reviews have no writing authority. Verification is proportional to touched paths; batch any Windows Luna handoff only when a coherent candidate needs it.
Done for next: Demonstrated numeric interaction gaps repaired with focused tests, current requirements and concrete next work saved, exact development commit verified remotely. Trial review does not block this checkpoint.
State: Local cloud snapshot nect-dot-takeover (not a full Git checkout); development branch codex/desktop-production-20261016 remotely verified at 411303cb2735e0bc2449ef95f776d01bd28cb106 before this repair. Cloud execution recovered. The latest WS verification ran no tests because its isolated checkout failed with NtCreateDirectoryObject 0xC0000022; do not count it as a pass or retry without new evidence. Delivered trial remains 7496aee. Former WS source owner remains released.
Authority: Shiro explicitly requested dot-side inspection, Brief creation and independently actionable fixes on 2026-10-17 09:16 JST. Prior code/test/doc/dev-branch/push/Draft PR permission remains. No main merge, formal release, force push, authentication, permissions or billing changes. Other stopped missions are not reopened by this Nect instruction.
Handoff: CONTINUE_CURRENT_TASK. Read logs one-way; do not start status-only Codex turns. See desktop/docs/editing-refinement.md and requirements-crosswalk.md.
