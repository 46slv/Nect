# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: the current user directs dot to take over the stopped development, prioritizing practical panels and color. Edits, tests, coherent commits and non-force development-branch pushes are authorized. Mainline integration requires its own exact-stack checks and review; release, force/history rewrite, credential or billing changes and bypass of safety denials are excluded. Product decisions remain in [Confirmed requirements](https://app.notion.com/p/01b2ff7802fc410ab4cdb8fd26264495); [REQ-211](https://app.notion.com/p/3fbfd279a6f381a9a7c9c36f5089fba1) governs color and the current [Workspace action](https://app.notion.com/p/40ffd279a6f38132856ce92091df60ea) governs the approved staged panel work. Research plans are not adopted product presets.

Coarse route: panels and color → remaining adopted Session, document, shape and text requirements chosen from live dependencies → measured native stabilization and Shiro review.

## ACTIVE CHECKPOINT — Graph Copy survives same-Composition History

Goal: repair [#129](https://github.com/46slv/Nect/issues/129): production CompositionSessionOwner must keep the captured graph clipboard across same-active-Composition Undo/Redo without weakening reference identity, failure atomicity or clearing boundaries.

Current phase: implementation and focused verification on `dot/graph-clipboard-history-20261109`, isolated from main `80781bfb2f042dfbdafc08a92128c95078f9f905`. Fresh Space/Home/Completion Route, adopted Graph Copy contract and GitHub owner/PR checks selected this non-overlapping daily-editing repair. No active #129 repair owner or PR was found. Panels/color remain the production priority; physical acceptance is a separate local wait.

Proven: new production-owner regressions reproduce Copy→Undo→Paste and Copy→Undo→Redo→Paste as empty-clipboard failures on the unchanged base. The candidate retains the private Copy snapshot and Paste metadata under an opaque project-owner token only for same-Composition History replacement. It checks previous Color binding/epoch and foreign Artwork targets in both snapshots before renewing context; expired references never revive. Captured values, fresh IDs/internal edges, relative placement, incremental offset, one-step Undo, prepared History/cancellation, failed-Paste Redo preservation, tab/fallback/new-owner clearing and reference expiry have focused coverage. Exact-candidate aggregate and Chrome receipts are pending; focused results are not a full-suite or native pass.

Next task: finish independent review, required checks and the existing production-keyboard browser regression; preserve the exact candidate in a Draft PR, then verify its full CI before integration judgement. Do not repeat the completed #229/#230 work or the rejected Stage 0 performance candidate.

Approach: one source writer for this bounded core/owner/test/documentation scope. The constructor accepts only another private Session with the matching non-null owner token; no clipboard payload export or post-construction setter is added. Switching outer Compositions, History fallback to a different Composition, New/Open/reload and import replacement still clear Copy. No schema, persisted state, OS clipboard, GUI layout or cross-document transfer is added.

Done for next: exact source/test receipts and remote head/tree agree; review blockers are resolved; required CI status is explicit. A bounded Graph Copy repair does not close the whole Mission.

State: independent cloud worktree based on main80781bfb; no native app launch/install/profile change. Desktop command tooling is quota-paused with no retry/reconnect. SHIRO's earlier #230 worktree/process cleanup remains a separate unresolved obligation. Cloud browser launch is host-socket-blocked; no sandbox weakening. The local aggregate's known base-reproduced artwork-duplication-dom heap failure must remain distinct from authoritative Windows CI.

Previous accepted evidence: [#230](https://github.com/46slv/Nect/pull/230) normally merged at80781bfb; #137 closed. Exact-merge [Core42550711253](https://github.com/46slv/Nect/actions/runs/42550711253) passed Linux/Windows and [Windows42550711102](https://github.com/46slv/Nect/actions/runs/42550711102) passed Unit/DOM3871 with four skips, Rust73 and Chrome114/114. Earlier accepted native ORBIT artwork evidence remains source-bound to66761990 in desktop/test-results/native-artwork-acceptance-20261107/run-0400/result.md. It does not prove this later source or clear physical acceptance.

Residuals: physical/human IME, OS color picker, native dirty-close/save-pending, linked/Capture, Asset chooser/WebView2, fixed-panel visual acceptance, malformed/disk-failure paths, installer/signing/distribution and Shiro's subjective review remain unverified. Large-scene performance and broader REQ-2/3/34/211 remain incomplete.

Authority: current user takeover/delegation and Mission permit bounded implementation, tests, documentation, coherent commits, non-force development pushes and normal main integration after differences/tests/ownership/stops are checked. No release, force/history rewrite, protection/security/billing changes, PR approval review or safety-denial bypass.

Handoff: CONTINUE_CURRENT_TASK.
