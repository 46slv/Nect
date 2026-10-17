# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT

Goal: Continue practical non-destructive vector modifiers and useful luminance mapping under Shiro's repeated request for Illustrator/Blender-style capabilities. Shiro asked for more capabilities again on 2026-10-17 at 09:37 UTC; dot selected a small variable-width stroke slice and a bounded image-input design.

Current implementation: catalog remains 23 additions. Grid Origin/schema15 is published at 0ada93c6ccca1b873dc96a82582870580763ab63 (94 focused checks, formatting and Web build pass). Whole-artwork Move guidance was clarified at 8f68d538d0b9948786a4a953b137945dd8db9093 (28 focused checks), without expanding movement support. Expand Stroke now has start/end width multipliers 0..4, default1, with a linear arc-length ramp on open contours; equal values reuse constant width including closed contours. Schema16 migration preserves defaults. Final focused geometry/core/DOM run passes28/28, formatting and Web build pass. The12-layer variable-stroke example round-trips identically and its actual SVG/PNG was inspected; proof was delivered at09:47 UTC. No full-suite/product-completion claim.

Frozen handoffs: native and original browser trial182ab49 contain19 additions/schema12; optional browser43970b1 contains23/schema14. Both remain unchanged. Native182ab49 opened/closed in an isolated profile at08:31 UTC. Source-bound browser QA of exact43970b1 passed at09:35 UTC: Chrome startup, Halftone pitch8→9, Undo/Redo, JSON download/reopen retaining9, and Line Screen/tangent examples. Exact Library ZIP startup remains unverified because Windows materialization failed on metadata support. Native Open/Save, physical IME and high-DPI remain unverified. Older trial binaries are not promised to read new schema files; preserve separate saved copies.

Variable width is published at 27362c85cda284557a28f018f7a2ef0af1b1796a. Image inputs are now implemented for Halftone, Ordered Dither and Line Screen only, using lazy bounded PNG sampling (8-bit noninterlaced RGB/RGBA, 4MP cap, Pako 3.0.2 streaming inflate and single 16MB identity cache). Image-source bypass is explicitly refused; unrelated Image-to-vector sockets remain invalid. Schema17 marks the new connection contract. Focused integration/helper/related checks pass45/45, formatting and Web build pass. The15-layer PNG example reopens identically and actual SVG/PNG was inspected; proof was delivered at10:05 UTC. Frozen trial packages remain unchanged.
Next: Publish image-input support and prepare the next bounded browser trial with these completed additions. Keep generated painted geometry composable through an explicit geometry conversion rather than widening all sockets. Arbitrary middle-width points and saved profiles remain later proposals. Do not restart completed packaging or duplicate the finished source-bound QA.

Approach: Common Session commands, deterministic identity, bounded geometry, one writer per source scope. Reuse prior evidence; focused tests and one build after each actual slice, no broad repeat audits. Keep execution/verification/artifact delivery distinct. The cloud snapshot has no Git metadata; publication uses expected-SHA development-branch updates and remote verification.

Authority: Existing Nect source/tests/docs/development-branch/push/Draft PR permission applies. No protected-main merge, formal release, force/history rewrite, authentication, permissions or billing changes. Other stopped missions remain stopped.

Handoff: CONTINUE_CURRENT_TASK. See desktop/docs/modifiers.md, editing-refinement.md, modifier-next-brief.md and image-luminance-brief.md. Image-input support is a bounded tested slice; broader PNG variants and other raster formats remain unsupported by the pure sampling adapter.
