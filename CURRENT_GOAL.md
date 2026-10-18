# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — Repeater/Snap integration batch (2026-10-18)

Goal: Integrate the parent-confirmed Repeater and pending 45-degree Snap into
codex/integrate-parallel-20261018, preserving the delivered SVG/Ctrl+A/grapheme/
per-operation recovery/Dither functionality. Build the next EXE once both pass.
Current phase: Repeater source integration and focused verification complete;
Snap exact commit is pending parent delivery. No next-batch EXE has been built.
Proven: Repeater integration source d3d11d16c391b49814020915ef08ce74dace68b5.
Original candidates 1fa6f2d -> 2d4586b -> 8d55121 were applied in order.
Core schema-list and main import conflicts were resolved retaining both lanes.
Repeater owns schema37 through one module constant; Dither remains schema36.
Legacy Repeater controls backfill under source versions below37 without changing
saved schema. Selecting ae-2d promotes to37. Current schema37 must contain all
new controls; older ae-2d project versions reject explicitly.

23 Repeater/creation/Mirror/Join tests,97 recovery/Text/grapheme tests and70
Dither/Points tests pass (190 total). Added mixed schema36 Dither + legacy
Repeater migration/promotion/Undo/Redo/exact JSON regression. Six production-CSP
browser paths pass: Repeater, Points, Dither, clipboard, grapheme-app and
per-operation recovery. Repeater Canvas/SVG/PNG all160x100 RGBA hashes equal
 d0ab49573707fe4b7005e49a21f8766b8d8f2ffd278f68925b2846139854dd4b.
Production Vite build passed121 modules. Scoped formatting and diff check pass.
Known Dither exclusions remain the documented baseline pinned-fingerprint and
Windows CLI cwd cases; no full-suite or physical-IME claim.
Evidence: desktop/test-results/integration/repeater-*.log and
{repeater-ae,dither-controls,node-clipboard,grapheme-app,recovery-per-operation}.

Next: receive parent-confirmed Snap commit from its dedicated lane, integrate
only shared hookups, run motion/focus/Undo and nearest affected regressions,
then one combined EXE build + isolated native acceptance + private Library ZIP.
Pending action: next-batch EXE build. Reason: required Snap commit has not arrived;
status PENDING_DEPENDENCY, not denied or an uncertain side effect.
No other Repeater implementation blocker remains. Do not duplicate Snap work.

Prior delivered batch remains source973486c3733916e738df8e5088f50c48e4743730:
EXE9,961,472 bytes, SHA256
1AF50B17350FC07C8EDF69FD520FD8E2AE7AA3280A117919A52B26014692E577.
ZIP5,248,465 bytes, SHA256
6120662CD42B4D33AD1FAD8AF8C38FC456BB24DB5D7E618174C8C33624E1E8F4.
Parent already delivered it. Repeater/Snap are not in that earlier EXE.

Authority: parent-delegated isolated source integration, normal branch commit/
push and one next-batch EXE after both features pass. No main/protected merge,
release, force push, account/auth change or user app/profile/recovery operation.
Gate: prior review workflow reused; local migration, renderer and browser
verification resolves this integration. Consultation not-required, external0.
Handoff: CONTINUE_CURRENT_TASK; parent supplies the exact Snap candidate.
