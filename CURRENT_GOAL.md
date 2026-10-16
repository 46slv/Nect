# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT

Goal: Finish the approved practical editing fixes: AE-style Transform, usable Path Remap controls, Alt-wheel zoom, numeric scrubbing consistency and editor context menus.
Current phase: dot cloud implementation. Former WS Owner released source/GUI at 2026-10-17 05:58:56 JST; checkpoint 462828e614eb86fe85469eb58faadc1f22170d44 independently materialized and Git-blob verified (328 files).
Proven: Prior delivered d6223e9 native qualification remains separate. WIP focused unit slice 17/17 passes locally; combined UI acceptance is pending.
Next task: Finish numeric transaction integration and test the real DOM/browser paths, then qualify Transform/Remap/zoom and exports. Update the compact specification with implementation decisions.
Approach: Reuse existing Session and shared affine; preserve legacy files, connections, expressions and locks. No general Mission expansion. New user requests may be integrated while dot owns implementation; consolidate before any Codex handoff.
Done for next: Approved fixes demonstrated, focused regressions and Web build passed, exact checkpoint preserved. Windows build/native testing and private trial delivery only when actually available; never equate browser proof to native proof.
State: Local cloud snapshot nect-dot-takeover, base 462828e614eb86fe85469eb58faadc1f22170d44; not a full Git checkout. Root dot is sole source writer. WS Owner is stopped. See desktop/docs/editing-refinement.md.
Authority: User 2026-10-16 20:34 approved Transform; 20:40–20:50 added interaction fixes and dot takeover, 20:51 approved handoff resend; 21:03 explicitly permits dot to revise implementation, Brief and specification. No main merge, release, force push, security or billing changes.
Handoff: CONTINUE_CURRENT_TASK. Do not start remote status-only turns. Observe formal logs; send only new requirements or concrete corrective actions. EPHEMERA and Resolve remain stopped.
