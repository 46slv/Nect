# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — Offline update identity and metadata validation

Goal: Implement the newly authorized bounded offline identity, pure validator and informational UI slice. Actual updater application remains outside scope.

Current phase: Resumed same single implementation writer. Published the preserved provider/notes work bounds normally at remote118c77d, matching local7145d48 tree1c5a3427. Recovered terminal old-run diagnostics and applied its remaining exact Rust formatting changes plus two Build-menu expectation repairs.

Proven: Native-authoritative read-only identity query, strict exact build version checks, pure assessIdentity/validateDiscovery/validateVersionSet and small Build menu implemented. Production provider is null. Focused tests, related Recovery/menu/lifecycle/editor DOM tests, both frontend modes and eight standalone Rust checks passed, with one inherited symlink fixture skip. Exact commands, results and boundaries are in desktop/docs/offline-update-identity.md.

Next task: Publish the bounded exact-CI formatting/menu-test repair on the same PR82 branch with expected remote118c77daab9a202e7b0adb6ce093f97fe9add530, verify the new remote tree and CI formatting/test results, then return the accepted source tree and explicit remaining gaps. Do not hold a completed follow-up merely to preserve an older CI.

Approach: Immutable native facts come from compiled version/features/protocol and successfully prepared RunStorage. Channel/package provenance stays unknown. Closed source-scoped test fixtures produce literal advisory metadata only; no production fixture setting or transport exists.

Done for next: Concrete reviewed diff, exact branch/head/tree, executed check evidence, development PR and explicit native acceptance gaps delivered to parent.

State: Writable isolated task-21 checkout, same branch and single writer. Terminal old CI preserved: native compilation passed, Rust44/44, frontend2345 pass/2 menu-expectation failures/4 skip. Exact formatter diff is recovered and applied. An unchanged browser atomicity helper produced a separate diagnostic mismatch; no lifecycle product code was changed. Local Cargo cache/rustfmt limitations remain recorded.

Authority: Current delegated instruction and trusted user authorization permit this bounded implementation, development commit/push/draft PR. No protected main merge, release, force/history rewrite, updater application, network/feed/provider settings, download URL, updater plugin, background task, new permission/CSP relaxation/version bump or document/session/lifecycle/preferences changes.

Handoff: CONTINUE_CURRENT_TASK through normal publication and new exact-head CI readback, then return this bounded slice. Native UI NOT_RUN. No updater application, production provider, lifecycle or session scope expansion.
