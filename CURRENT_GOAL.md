# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Test extraction and Windows CLI failure repair (2026-10-19)

Goal: Separate test-module extraction defects from production defects, repair the affected DOM fixtures and Windows file-URL handling, and commit the targeted cleanup locally.

Current phase: Fixture fixes and isolated test runs are complete; checkpoint documentation is updated and a local commit remains.

Proven:

- In production `main.js`, project persistence uses `serializeProject(session.projectSnapshot())`; the injected DOM helper's `serialize(history.doc)` referenced a name outside its data-URL module. The fixture now uses the production project serializer.
- Production imports and passes all recovery functions to `RecoveryAutosave`. Three tests replace `storage.js` without retaining those names; their isolated storage stubs now include the recovery surface.
- Production imports `evaluateCore` and declares its own local `evaluate` wrapper. The snap test injected a second `evaluate`; it now instruments the existing wrapper.
- Windows child-process tests now convert script and working-directory URLs with `fileURLToPath`, while launching with `process.execPath`.
- Focused results: `artwork-snap-dom` **1 PASS**; `system-font-dom` **1 PASS**; `variable-font-ui` **1 PASS**; `variable-font-cli` **5 PASS**. The repaired image CLI comparison **1 PASS**. `blank-artwork-dom` has 8 PASS and one remaining assertion failure (the parent suite also fails): expected 2 layers, got 1. `rich-text-dom` has one remaining assertion failure (24 vs 5). `image-luminance` has 10 PASS and one remaining default-vector fingerprint mismatch (expected `sha256:85550975e61ac8b2b1fef571d2356555fdec3924cefa07f82ffeb4978322e3d0`, got `sha256:93ed6708de2add4813a461700e772d8b5a02c9de115ef84f9a51e1ac31624ff0`). Their relation to product behavior requires separate classification.
- `git diff --check`: **PASS**. No full-suite run, network retry, or native/real-browser runtime validation was performed.

Next task: Classify the remaining blank-artwork layer-count, rich-text metric, and vector-fingerprint failures against current Composition behavior and accepted contracts; then resume native/browser Open, Save, recovery, and public-load acceptance for multi-Composition projects.

Approach: Keep this checkpoint's edits in test fixtures only. Investigate each remaining assertion independently before changing production behavior or expected values. Run the actual application acceptance only when its runtime is available.

Done for next: The extraction and Windows-path failures are corrected, the current target evidence is recorded, and this local checkpoint is committed without pushing.

State: Branch `codex/nect-regression-validation-v2`; base HEAD `f1a6dd143e23ede7b2b7037ae56cf8388b7667db`; this checkpoint changes seven test files plus this record. Dependencies needed by the selected Node tests were available locally. Native runtime remains unverified. No network, push, or permission changes.

Authority: User's follow-up authorizes these localized fixes, targeted tests, and a local commit; it forbids network retry, push, and runtime-access bypass.

Handoff: CONTINUE_CURRENT_TASK
