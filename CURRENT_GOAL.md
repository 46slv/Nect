# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Classify residual DOM and fingerprint failures (2026-10-19)

Goal: Determine whether the blank-artwork, rich-text schema, and vector-fingerprint failures indicate product defects; fix confirmed fixture defects without weakening the intended contracts.

Current phase: The two DOM failures are corrected in fixtures and their focused tests pass. The vector fingerprint assertion is a known stale baseline golden and remains unchanged. Local commit is pending.

Proven:

- Blank-artwork failure occurred on the Text iteration after New. REQ-210 specifies that a normal Text Tool click selects the variant and a primary Canvas press/release creates Text. The fixture selected Text but never made a valid primary canvas click; Happy DOM also provided a zero-sized stage. It now emits primary pointer events, supplies realistic stage bounds, and clicks the Canvas. `blank-artwork-dom.test.js`: **10/10 PASS**; `text-tool-dom.test.js`: **5/5 PASS**. This was not an active-Composition read/write defect.
- The UI creates artwork through `session.dispatch`; the public `win.nect.document` getter calls the session snapshot, and `CompositionSessionOwner.snapshot()` delegates to the active CommandSession. The targeted model/owner tests pass **20/20**, including legacy JSON migration, multiple Composition roundtrip, active-only edits, cross-Composition Undo/Redo, and switching without a history step. The actual native/WebView Composition selector flow remains unverified.
- The rich-text assertion assumed a new typography edit promotes every document to schema 5. The default `document()` schema moved to 24 in commit `8c5b075`, while `updateTextTypography` correctly uses `max(existing, 5)`. The fixture now explicitly loads schema 2, verifies it remains 2 on read, and verifies the edit promotes it to 5. `rich-text-dom.test.js`: **1/1 PASS**.
- The pinned vector fingerprint mismatch is documented in `desktop/docs/dither-controls.md` as already failing against the unchanged baseline; the doc records the old golden as stale and leaves it intact. The current actual value remains `sha256:93ed6708de2add4813a461700e772d8b5a02c9de115ef84f9a51e1ac31624ff0`; its preceding area assertion passes at 256. Current default tone power and legacy Halftone mode are omitted from fingerprint additions. No production change or expected-hash update is justified here.
- `git diff --check`: **PASS**. No full-suite run, network retry, or real browser/native runtime validation was performed.

Next task: Resume the Composition lifecycle acceptance for Open, Save, recovery, and public load when the app runtime is available. Keep the documented stale vector golden visible as a known test failure rather than rewriting it without a baseline correction decision.

Approach: Preserve the schema-2 legacy fixture and primary Canvas Text Tool sequence. Continue with the current Composition owner as the sole session/history authority; validate inactive Compositions through project serialization and targeted owner tests.

Done for next: Both fixture failures now exercise their accepted contracts and pass; the fingerprint assertion is classified as pre-existing stale golden and left unchanged; this local checkpoint is committed without pushing.

State: Branch `codex/nect-regression-validation-v2`; base HEAD `e59d4a0e0a30f5bbb617174efac2ec53489a1caf`; this checkpoint changes two test fixtures plus this record. Selected Node tests ran with locally available dependencies. Native/real-browser runtime remains unverified. No network, push, or permission changes.

Authority: User's follow-up authorizes classifying these assertions, localized test fixes, targeted verification, and a local commit; no full audit or repeated full suite.

Handoff: CONTINUE_CURRENT_TASK
