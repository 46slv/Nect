# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — direct path editing and endpoint continuation

Goal: Complete the user-authorized A direct selection, selected-point nudge,
Alt independent handle and open-end Pen continuation. Preserve one-operation
Undo, stable point IDs, generated Remap provenance, locks and point Delete ownership.

Current phase: A, point nudge and independent handles are implemented and pass
model and production UI acceptance; preserve this checkpoint, then implement the
remaining endpoint continuation. Existing Alt anchor-to-paired-handles behavior
is retained. Existing Pen creates independent artwork; it does not resume an
existing endpoint yet. No duplicate replacement implementation is needed.

Proven: HEAD f055541 preserves OS Text horizontal wrap (schema32). Library
libfile_0b7707ec405c8191afbd1120bb51e0eb, ZIP SHA256
BBAAD76FFF7CB893CAB7B4A071AC3B3C3E55C23EAA2263D453DD42ABED0444ED;
PR24 issuecomment-6246741306. Includes previous Composition/DIN/axes/vertical
checkpoints. Native34, wrap/related45 and production native-fixture UI passed.
New path work: model/gesture/deletion14 passed. Production Edge/CSP passed A,
actual transformed point drag, world 1px/Shift10px and multi-point nudges, Alt
one-handle separation with opposite vector unchanged, Undo, locks, IME/input focus,
toolbar Delete safety and protection of procedural sources; receipt in
 desktop/test-results/direct-selection/receipt.json (generated/ignored).

Next task: Add common-session Pen continuation for explicit owned open endpoint
IDs, preserving existing point records and all unrelated nodes/contours. Refuse
closed/interior/locked/driven/generated/suspended/topology-sensitive targets.
Verify tail/head continuation, cancel/zero/stale handling and one Undo, then
produce a separate combined path checkpoint and continue eligible work.

Approach: A resolves one unique Output/paint/literal Transform route to an owned
Curve/Multi Path or explicit Remap, maps displayed affine geometry to authored
point-space deltas and refuses branching/driven/singular cases. It never creates
or bakes sources. translate-points uses the same common gesture authoring path;
Remap binds current source fingerprint and refuses suspended edits. Alt handle
movement sets corner mode and one vector atomically. Point commands retain only
still-valid point selection on Undo; A prevents Delete from becoming node deletion
when focus is on a toolbar. No saved-format change for these path interactions.

Done for next: All four requested interactions work through Canvas and common
Session with focused regression, reproducible tests and preserved earlier packages.
State: codex/desktop-production-20261016, base f055541; path changes uncommitted.

LOCAL_WAIT: Actual native WebView2 IME/high-DPI/full manual editing. Formal native
UI controller is unavailable; no repeated inventory without new evidence. This
does not stop source implementation. OS full Japanese kinsoku/vertical wrap,
custom leading/rich/path/character adjustments and color/bitmap remain explicit
limits; no universal font-completeness claim.

Authority: User-approved edits/tests/commit/non-force dev push/PR/EXE/ZIP/Library.
No main merge/release/force push, auth/permission changes, font-cache/software
extraction or Notion mutation. Parent owns Notion and user delivery.

Double-check: local critic for interaction/authority boundary; exact stable IDs,
affine inverse, generated fingerprints and transactional counterexamples verified.
Consultation not-required, zero external sends; continue focused local evidence.
Handoff: CONTINUE_CURRENT_TASK. Commits and distributions are checkpoints, not stops.
