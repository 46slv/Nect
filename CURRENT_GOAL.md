# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Offline preview update check

Goal: Add an offline preview metadata envelope with injected time/expiry validation and a fixture-only check/retry UI, reusing existing identity, scope, version and literal rendering rules.

Current phase: Sole writer on a separate branch from the current main tree. PR84 remains unchanged and is not a dependency. Focused tests passed; final aggregate checks and draft publication follow.

Proven: Production provider remains null. The new fixture client is outside the production import graph. Existing schema1 validation remains unchanged; schema2 accepts only an expiring envelope around it. No source, clock, identity or request factory is obtained implicitly.

Next task: Complete focused/aggregate checks, preserve exact source and test evidence, then publish a development commit and draft PR. Report actual CI separately from local fixtures.

Approach: Reuse validateDiscovery and renderDiscovery. Require native preview eligibility, exact audience, positive safe-integer expiry and injected safe-integer time. Fail closed for malformed/expired metadata. Clear stale advisory during an explicit check, permit a fresh-request retry after failure, ignore overlapping and disposed results.

Done for next: Reviewable diff, exact branch/head/tree, commands/results and draft PR; explicit remaining native UI and live distribution dependencies.

State: codex/preview-discovery-offline-20261024; local source base ff8bceabc3082766a2219c922d3781dba4d2128e has tree2eee60355c3179b9a14aed2557b6f2a9d2eaaeb5, identical to remote main d0d9c9265a1628983f727715bc205a44aaa5c0a6. Other worktrees are preserved.

Authority: Current delegated preview-increment instruction and standing Nect development permission cover edits/tests/commit/normal push/draft PR. No main merge, release, live provider/feed, download/install/restart, keys, credentials, CSP/security, lifecycle/preferences or user-session changes.

Handoff: CONTINUE_CURRENT_TASK through draft publication and exact-head check evidence. Native UI NOT_RUN remains a local acceptance wait; live delivery needs explicit native preview version/channel, durable distribution, signing/bootstrap and safe apply/rollback decisions. See desktop/docs/preview-update-check.md.
