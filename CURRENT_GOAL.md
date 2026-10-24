# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Joystick viewport direction

Goal: Make the Graph joystick move the viewport toward the tilt direction, so visible content moves the opposite way; keep the knob tracking the finger.

Current phase: Sole writer on codex/joystick-screen-direction-20261024 from the current main tree. Offline preview work is preserved separately in draft PR86, and PR84 is unchanged. The user's explicit viewport clarification supersedes the earlier content-motion interpretation.

Proven: The old content-grab sign fails the new shipped-app directional assertion; the final corrected source passes29 focused controller/app/other-navigation tests. Four axes and all four diagonals cover mouse and touch, document/history neutrality, knob tracking, fractional accumulation, bounds and stop paths. Full frontend aggregate passed2363/0fail/5skip, format/config/build passed. Local Chrome launch closed before page creation; real-browser behavior and native UI are NOT_RUN locally and must be established separately by CI/acceptance.

Next task: Complete aggregate/build/format and real-browser verification where available, preserve the exact candidate, publish a separate development commit and draft PR. Continue PR86 exact-head CI observation without changing its source.

Approach: Change only the joystick's viewport adapter from subtracting to adding its CSS-pixel delta. Keep the input vector, speed/deadzone/capture/release/Cancel logic and ordinary drag-pan unchanged. Assert real-browser node rectangle movement opposite the viewport in all eight directions.

Done for next: Separate reviewable diff/branch/head/tree/draft PR, validation results and explicit screenshot/native UI gaps.

State: Local base ff8bceabc3082766a2219c922d3781dba4d2128e/tree2eee60355c3179b9a14aed2557b6f2a9d2eaaeb5 is identical to remote main d0d9c9265a1628983f727715bc205a44aaa5c0a6. Other worktrees are preserved.

Authority: Current delegated priority fix and later trusted user clarification authorize scoped implementation/testing/commit/normal push/draft PR. No main merge, release/install, updater/provider, credentials, settings/security or user-session mutation.

Handoff: CONTINUE_CURRENT_TASK. Supported image preparation succeeded but materialization failed first during download, then once during unsupported Windows os.setxattr; no readable image remained. Screenshot NOT_INSPECTED, native UI NOT_RUN. Explicit viewport semantics remove the screenshot requirement as an implementation blocker. See desktop/docs/graph-joystick-navigation.md.
