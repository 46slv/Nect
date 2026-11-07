# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: the current user directs dot to take over the stopped development, prioritizing practical panels and color. Edits, tests, coherent commits and non-force development-branch pushes are authorized. Mainline integration requires its own exact-stack checks and review; release, force/history rewrite, credential or billing changes and bypass of safety denials are excluded. Product decisions remain in [Confirmed requirements](https://app.notion.com/p/01b2ff7802fc410ab4cdb8fd26264495); [REQ-211](https://app.notion.com/p/3fbfd279a6f381a9a7c9c36f5089fba1) governs color and the current [Workspace action](https://app.notion.com/p/40ffd279a6f38132856ce92091df60ea) governs the approved staged panel work. Research plans are not adopted product presets.

Coarse route: panels and color → remaining adopted Session, document, shape and text requirements chosen from live dependencies → measured native stabilization and Shiro review.

## ACTIVE CHECKPOINT — production workflow continuation

Goal: practical fixed panels and color that remain editable through artwork creation, Undo/Redo, Save/Open and later re-edit, with common Session commands and stable authored identity.

Current phase: dot takeover and recovered branches through #221 are integrated. The stopped Codex writer stays stopped. Latest application integration is main 04628a4f7185ed26490b5e3ee14b0c5a94b704bb, tree 973e62628a413e855562dcf57ec75775a974dbd1, identical to #221's fully checked candidate. Mainline integration is complete, not waiting for that PR's CI. Separate post-merge run status is tracked in the [#221 receipt](https://github.com/46slv/Nect/pull/221).

Proven: #221 candidate 7390a6b4 passed both Core checks and Windows 42323434212: 3,851 frontend Unit/DOM passed, zero failures and four existing skips; Rust 73 passed; actual Chrome 114/114 scripts passed. The recovered #91 browser picker cleanup is included as unchanged source ancestry; the old branch is preserved. Fresh focused checks passed 93 tests, build/native-CSP and four browser scripts. The picker regression fails on the former main and passes with the recovered claim-once cleanup. Current Session/project revision guards and explicit Capture are unchanged.

Artwork evidence: original ORBIT manual colors, opt-in linked-B editing, and existing-palette explicit Composition Capture have passed browser-process exit/restart and later re-edit/export. The latest Capture test preserved the receiving 16-layer Composition, packet and ten slots, remapped five ordinary colors, retained exact local captured values and full 960x540 pixels, and kept captured output unchanged when the receiving live palette was later edited. Atomic Undo/Redo, editable JSON, SVG/PNG, unchanged original/input hashes and zero page errors were verified. See desktop/docs/color-composition-capture-restart.md and the preceding linked/manual receipts referenced in Git history. These are browser and explicitly experimental copied-artwork checks, not final presets or native acceptance.

Next task: continue the highest-value independent accepted production obligation. Measure the remaining 10,000-point cross-Composition edit latency on the current source, identify its actual bottleneck and make a bounded repair if justified, preserving Session/history/reference semantics. Candidate and post-merge CI reconciliation continues separately; a native GUI wait does not block independent work.

Approach: one source writer per scope, isolated branches, exact-candidate checks and ordinary non-force Git. Panels ship at fixed positions; development-time arrangement improvements do not authorize user-configurable docking. Held drawer-count decisions and final experimental coefficients are not defaults. Preserve original branches/data and stop instructions.

Done for next: advance an accepted production obligation with source-bound measurement/verification and a durable result, or record its exact actionable blocker. The overall Mission remains incomplete.

State: primary D:/Documents/Codex/2026-11-02/task/Nect-pr178-combination, production branch normally synchronized to application main 04628a4f and clean. #221/#91 are merged; original heads 7390a6b4/830d64b and all previously recovered branches remain preserved. Import source ownership is released. This documentation-only checkpoint does not change application files; read Git for the current documentation HEAD.

Residuals: native WebView2, physical IME, OS color picker/OS Cancel and subjective review remain LOCAL_WAIT until GUI controller ownership has new evidence. Do not repeat old failed controller inventories. Final experimental presets, legacy palette migration, broader transfer/ambiguous clipboard provenance, measured large-scene performance and full REQ-2/3/34/211 remain incomplete. Image Luminance remains internal/unconnected, with no public node or full Field-workflow claim.

Authority: current user takeover/delegation and Mission permit bounded implementation, tests, documentation, coherent commits, non-force development pushes and normal main integration after current differences/tests/ownership/stops are checked. No release, force/history rewrite, protection/security/billing changes, PR approval review or safety-denial bypass.

Handoff: CONTINUE_CURRENT_TASK.
