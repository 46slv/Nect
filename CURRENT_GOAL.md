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

Current phase: Single implementation writer on isolated branch codex/offline-update-identity-20261023, based on current main 282b325dd01608fbd5366c026ced13f8ba78605c. Initial 1,332 blobs matched the supported GitHub tree and initial Git status was clean. No equivalent active implementation or PR was found in supported ownership logs.

Proven: Native-authoritative read-only identity query, strict exact build version checks, pure assessIdentity/validateDiscovery/validateVersionSet and small Build menu implemented. Production provider is null. Focused tests, related Recovery/menu/lifecycle/editor DOM tests, both frontend modes and eight standalone Rust checks passed, with one inherited symlink fixture skip. Exact commands, results and boundaries are in desktop/docs/offline-update-identity.md.

Next task: Draft PR82 is published at c8f0e11cd1a6fe4b3955a65f53661d83065658ce, tree 445756c68d5b2b94e951a5b97c4233bc367c39f8. Its native compilation and Rust test stages passed; formatting failed and the full frontend stage is still running. Read completed job120452801263 logs, reconcile exact formatting, then fast-forward the same branch with the preserved local provider/notes and formatting follow-up. Do not interrupt the progressing CI or create another writer. Local full Cargo and rustfmt remain unavailable; native GUI/visual acceptance remains open.

Approach: Immutable native facts come from compiled version/features/protocol and successfully prepared RunStorage. Channel/package provenance stays unknown. Closed source-scoped test fixtures produce literal advisory metadata only; no production fixture setting or transport exists.

Done for next: Concrete reviewed diff, exact branch/head/tree, executed check evidence, development PR and explicit native acceptance gaps delivered to parent.

State: Writable isolated checkout under task-21/nect-offline-identity. Cargo application check stopped before compilation because offline rfd is unavailable; rustfmt is not installed. Denied D-drive Git reads and failed shell Git transports were stopped, without escalation. Supported GitHub API reads and blob-verified snapshot supply the source; no existing writer tree was edited.

Authority: Current delegated instruction and trusted user authorization permit this bounded implementation, development commit/push/draft PR. No protected main merge, release, force/history rewrite, updater application, network/feed/provider settings, download URL, updater plugin, background task, new permission/CSP relaxation/version bump or document/session/lifecycle/preferences changes.

Handoff: CHECKPOINT_READY. Return the published PR, exact local/remote source and preserved follow-up to parent. Fast-forward publication is pending the completed existing CI diagnostic; do not wait indefinitely or cancel the progressing command. Continue only this same bounded offline slice.
