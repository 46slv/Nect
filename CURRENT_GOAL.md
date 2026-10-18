# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — Repeater + Snap integrated trial delivered (2026-10-18)

Goal: Integrate parent-confirmed Repeater and45-degree Snap while retaining the
previous SVG/Ctrl+A/grapheme/per-operation recovery/Dither batch, then build one
combined EXE and privately deliver its trial ZIP. Current phase: completed.
Proven source: b1894b1a9eed292c9c2a1df4d0c20f620b691b2a on
codex/integrate-parallel-20261018. Repeater original1fa6f2d/2d4586b/8d55121
integrated with core schema-list and main import conflicts resolved retaining
both lanes. Repeater owns37, Dither remains36. Mixed migration/promotion/
Undo/reopen is verified. Snap original85e568e plus shared patch is connected;
three-way import resolution retains grapheme editing. Pure formatting hunks
were excluded. Snap changes no authored schema or new Guide/Grid type.

235 focused checks pass across Snap45, Repeater/creation/Mirror/Join23,
recovery/Text/grapheme97 and Dither/Points70. Eight final production-CSP browser
paths pass: Snap17 actual scenarios, Repeater full Canvas/SVG/PNG RGBA parity,
Points, Dither, grapheme-app, Graph clipboard, per-operation recovery and SVG.
Existing baseline Dither fingerprints/Windows CLI cwd and artwork-snap-dom
injection limitations remain explicitly excluded. No full-suite claim.
Production Vite build122 modules passed; scoped formatting and diff check pass.

Exactly one standalone EXE built for this batch. Actual WebView2 with verified
owned socket/PID ancestry, separate acceptance profile and recovery store passes:
Repeater Offset/Undo and per-copy opacity; Graph Shift45 visible guide + one
Undo; retained SVG Path/Join, Graph A/C/V/Undo, Points edit/Undo and grapheme
Backspace/Undo/Text commit. Real disk recovery equals latest JSON. A second fresh
profile restores via Recovery copy UI with exact JSON/SVG and unsaved/unbound
state. Both owned processes exited. Page errors empty. Screenshots inspected
locally; no browser screenshots included in the Library ZIP.
Evidence: desktop/test-results/{integration,snap-45,repeater-ae,dither-controls,
grapheme-app,node-clipboard,recovery-per-operation,svg-part}; task-local
native-next/{phase-1,phase-2,socket-1,socket-2}.json and private-profile outputs.

EXE9,964,544 bytes, SHA256
3E2682D7F3BCB306F55C19C9D18B9572EE3E1843DA9183C0098E2E12E33ABEF8.
ZIP Nect-Desktop-integrated-20261018-b1894b1.zip,5,107,965 bytes, SHA256
B40F4A3FCE10AE88B4761C9B0AF32602F6AFA1D1ACDDD3EE75885381AC2A2C60.
Public prior examples/licenses retained; editable Repeater example added.
Private Library create succeeded; identity/version retained in local delivery
receipt. Earlier973486c trial remains separate and unchanged.

Limits: integer Repeater Copies/Offset, no nested ae-2d/3D/timeline/painted-input
duplication; Snap extends existing positional routes only, with no authored
slanted Guide/Grid or numeric direction constraints. Existing shaping/font
coverage limits remain. Physical IME/high-DPI/pen and native file dialogs were
not exercised in this combined native run. No user app/profile/recovery/private
font or artwork operation, main/protected merge, release, installer, force push
or account/auth change.
State: source pushed; this final documentation checkpoint follows. No remaining
candidate dependency, implementation blocker or denied/pending/uncertain action.
Next: parent delivers the Library trial and chooses the next bounded checkpoint;
user logo work is independent of this integration.
Gate: review workflow reused; local migration, unit, pixel, browser and native
restart evidence resolves this batch. External consultation not-required, sends0.
Handoff: CONTINUE_CURRENT_TASK; parent coordinates next accepted work.
