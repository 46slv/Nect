# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: the current user directs dot to take over the stopped development, prioritizing practical panels and color. Edits, tests, coherent commits and non-force development-branch pushes are authorized. Mainline integration requires its own exact-stack checks and review; release, force/history rewrite, credential or billing changes and bypass of safety denials are excluded. Product decisions remain in [Confirmed requirements](https://app.notion.com/p/01b2ff7802fc410ab4cdb8fd26264495); [REQ-211](https://app.notion.com/p/3fbfd279a6f381a9a7c9c36f5089fba1) governs color and the current [Workspace action](https://app.notion.com/p/40ffd279a6f38132856ce92091df60ea) governs the approved staged panel work. Research plans are not adopted product presets.

Coarse route: panels and color → remaining adopted Session, document, shape and text requirements chosen from live dependencies → measured native stabilization and Shiro review.

## ACTIVE CHECKPOINT — same-project outer-tab Graph Copy

Goal: finish the bounded outer-Composition-tab self-contained Copy/Paste follow-up in [#129](https://github.com/46slv/Nect/issues/129), without widening cross-project/OS clipboard, reference rebinding or asset migration.

Current phase: implementation and verification on isolated `dot/project-graph-clipboard-20261109`, based on main `17101efadd8912a8cb8aad89a9df66dd77bdc7f4`. Fresh Home, REQ-292, issue129 follow-up and open-PR/owner checks confirm this existing same-project workflow and no duplicate repair writer. Historical Oct18 acceptance exercised Layer graphs called Compositions, not outer tabs, so new production-owner and real-tab evidence are required.

Proven: production sync/prepared A→B switches lost Copy on the unchanged base. The candidate transfers private snapshots only under the existing project-owner token and only self-contained payloads across outer IDs. Captured values/internal connections/relative placement, new node/Curve point IDs, incremental offsets, one-step Paste Undo, exact shared font/image/Color-slot IDs, no Copy/tab History or dirty changes, ordinary project operations and History fallback have focused coverage. A read-only freshness assertion captures document/revision/context and private clipboard metadata, so callbacks cannot replace a newer Copy or tab with an older candidate. Independent review exposed a final-quiescence reentrant-tab gap; owner authority is now rechecked after all final callbacks, with sync/prepared three-tab regressions.

Next task: finish independent review and full checks, preserve the exact candidate in a Draft PR, inspect exact-head Windows/Core CI and retained outer-tab keyboard receipt/PNG, then make an integration judgement within current authorization. Existing node-clipboard browser script is already registered as115 with retained PNG/JSON/SVG; this candidate extends it with real outer tabs and actual Copy/Paste/Undo/Redo. No old Layer-only or old-head result substitutes for that new evidence.

Approach: no raw clipboard getter/setter or second model. The read-only freshness closure does not expose payload data. Cross-tab external inputs, Artwork/Color Artwork references and legacy Layer-graph Composition References clear rather than retarget, even if destination IDs coincide. Same-Composition reference expiry rules remain. New/Open/reload, import replacement and failed rollback retain empty/reset boundaries. Only create/rename/resize/delete project replacements explicitly inherit; no blanket constructor default. Prepared bank activation remains the existing trusted final atomic bank/tab publication.

Done for next: exact-candidate tests, review, remote head/tree and real-tab receipt agree; required checks and any limitations are explicit. This is a bounded authoring repair, not whole-product or native/physical acceptance.

State: independent cloud worktree based on main17101ef; one source writer for core/owner/clipboard tests and related docs. No RDC, native app launch, install/profile or user workstation mutation. The user's no-RDC direction applies even if an old quota expires. Prior SHIRO worktree/process cleanup remains unresolved through separately authorized non-RDC routes. The cloud browser host restriction is not bypassed; actual Chrome evidence uses the existing CI lane.

Previous accepted evidence: [PR231](https://github.com/46slv/Nect/pull/231) normally merged17101ef, tree2c3180de, after verified readiness/merge authorization. Exact-merge Core42568208867 and Windows42568208930 succeeded: Unit3884 PASS/4 SKIP, Rust73, realChrome115/115 including actual-key same-Composition UndoRedo receipt. Issue129 was deliberately left OPEN for this outer-tab follow-up. Final local aggregate on that exact base tree had3879 PASS/1 baseline-reproduced artwork-duplication-dom heap-OOM FAIL, not a full local pass. Do not repeat that completed checkpoint or the rejected Stage0 performance candidate.

Residuals: physical/human IME, OS color picker, native dirty-close/save-pending, linked/Capture, Asset chooser/WebView2, fixed-panel visual acceptance, malformed/disk-failure paths, installer/signing/distribution and subjective review remain separate. Large-scene performance and broader REQ-2/3/34/211 remain incomplete.

Authority: current user delegation and confirmed Graph Copy requirement permit bounded implementation, tests, documentation, coherent commits and non-force development pushes. Normal integration requires current differences/tests/owner/stops/protection checks and applicable readiness authorization. No release, force/history rewrite, protection/security/billing changes, PR approval review, RDC or safety-denial bypass.

Handoff: CONTINUE_CURRENT_TASK.
