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

Current phase: All four interactions are implemented and the new Windows trial
is uploaded to Library. Endpoint continuation
passes common Session and production UI checks for both ends of Curve/Multi Path,
literal Transform placement, closure, cancellation, unchanged finish, Undo/Redo,
JSON reload and matching SVG/PNG pixels.
Existing independent Pen and Alt anchor-to-paired-handles behavior are retained.

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

Next task: Native hands-on acceptance of this combined checkpoint when the actual
WebView2 UI controller becomes available; no further code work is implied merely
by that local acceptance wait. Parent owns the next product-priority decision.
Endpoint continuation preserves existing point records and unrelated contours;
closed/interior/locked/driven/generated/retained-Remap targets reject atomically.

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
State: codex/desktop-production-20261016. Build source de2b5893580b8ba945c865b30b18b28c606d5ef9
includes A/nudge/Alt at 2f89184 and endpoint continuation. Combined model/DOM37,
production A/continuation UI and legacy Pen UI passed. New EXE startup responded
with no logs; six packaged examples re-export exact SVG offline. No schema change.
Library libfile_fae9c257178481919aa938af765d643a, ZIP 3,801,569 bytes:
SHA256 52D829077B0B46A8544F84FD9304DB9E9102BEBDF60E0BB06E39A368E11CBD7F.
EXE 9,772,032 bytes; SHA256
1DE122C1AC0F500D6B9BED2FA033AD6E7C39891552DD074EC732C8E0E28363A3.
Earlier schema32 packages remain preserved. Local artifact is task-root
Nect-Desktop-trial-20261018-de2b589.zip; BUILD/receipts are inside the ZIP.

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
