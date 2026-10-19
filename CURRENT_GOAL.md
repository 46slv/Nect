# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - SVG geometry precision (2026-10-19)

Goal: Offer an optional decimal precision setting for generated SVG geometry while preserving the byte-identical Default output, root sizing/viewBox relationship, and editable source project.

Current phase: Implementation, focused verification, records, and production build are complete; checkpoint commit and normal branch synchronization remain.

Proven:

- The selector is output-only (`Default` or 2–8 decimals). SVG numeric formatting is limited to known path/geometry/style fields; layer labels, text, IDs, colors, URL references, embedded image data, root physical dimensions, and canonical pixel `viewBox` are not rewritten.
- Default output matches byte-for-byte. Explicit precision rejects contour/image/mask/gradient/stroke cases that collapse geometry to zero. The live-Text/session test confirms serialization, History, Text content/type, and Undo/Redo remain unchanged.
- Focused SVG precision, root unit, embedded image, linear-gradient, and radial-gradient tests: 36/36 PASS. Syntax, scoped Prettier, and production build: PASS.
- Full `npm test` and actual WebView2/UI file inspection: NOT_RUN. No dependency install, network retry, or browser-access retry was made.

Next task: Commit and normally push this checkpoint, verify the remote branch HEAD equals local, and select the next export acceptance step from live route evidence.

Approach: Keep the selector on the export copy and use the default legacy path unless explicitly changed. Use local focused tests/build only; no dependency/network or denied browser retries.

Done for next: Default SVG remains byte-identical; explicit numeric precision affects only targeted output fields and refuses total small-geometry collapse; source/session state and root sizing remain unchanged; records are committed and synchronized.

State: Branch `codex/nect-regression-validation-v2`; starting HEAD `c173991c025dfb60ecaaf85ee989d9a7fc124da1`; working tree contains this scoped implementation/test/record change. Build output is ignored. Verify final local/remote HEAD and clean status at checkpoint closure.

Authority: The user's latest instruction explicitly authorizes this decimal precision output-copy option, limited implementation/tests/records, commit, and non-force development-branch push. Source, Undo history, live Text, merge/release, force push, credentials/permission changes, dependency/network retry, browser bypass, and Notion writes remain outside this task.

Handoff: CONTINUE_CURRENT_TASK
