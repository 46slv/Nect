# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT — 2026-10-28 Color coverage renderer bridge

Goal: Connect the accepted REQ-211 visible-contribution design to the existing renderer without inventing color identity or claiming unsupported attribution.

Current phase: Root is the sole source writer on codex/nect-color-core-20261028. Codex is used only for independent review or separated work, per the user's 10:54 UTC instruction.

Proven: Pure accounting at 6cdf51c has seven passing tests. PR #106 is independently frozen at 462eb8d; Windows CI 40695190430 succeeded and its four nested result files and nine screenshots were recovered. Native application acceptance remains NOT RUN. The local experimental bridge now emits separate solid Fill then Stroke SVG passes using actual exportScene/renderElement fixtures; five additional tests pass. No production editor imports this bridge.

Next task: Review the bridge; add actual raster sampling and compare total alpha with the real rendered scene. Preserve explicit source identity separately from strict Color values. Unbound paints remain Other; do not infer IDs from equal RGB. Groups, masks, gradients, raster and repeater attribution currently reject rather than silently produce partial percentages.

Done for next: Validated paint ordering, artboard clipping, opacity and provenance with bounded real pixel evidence before UI or persisted-schema integration.

State: Isolated color branch/worktree, base PR #106 at 462eb8d. Hermes is on explicit hold. User is organizing WS; cloud work continues while native acceptance alone waits. Existing PR #106 and PR #105 remain open and unmerged.

Authority: Accepted REQ-211 design handoff and user's direct Nect implementation instructions. Edits, tests, development commits/push and Draft PR are permitted. No protected-main merge, release, force push, credential/permission/billing change. EPHEMERA's rejected push/PR is not revived.

Handoff: CONTINUE_CURRENT_TASK. Root owns source; read-only reviewers do not edit this lane.
