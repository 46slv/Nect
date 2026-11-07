# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: the current user directs dot to take over the stopped development, prioritizing practical panels and color. Edits, tests, coherent commits and non-force development-branch pushes are authorized. Mainline integration requires its own exact-stack checks and review; release, force/history rewrite, credential or billing changes and bypass of safety denials are excluded. Product decisions remain in [Confirmed requirements](https://app.notion.com/p/01b2ff7802fc410ab4cdb8fd26264495); [REQ-211](https://app.notion.com/p/3fbfd279a6f381a9a7c9c36f5089fba1) governs color and the current [Workspace action](https://app.notion.com/p/40ffd279a6f38132856ce92091df60ea) governs the approved staged panel work. Research plans are not adopted product presets.

Coarse route: panels and color → remaining adopted Session, document, shape and text requirements chosen from live dependencies → measured native stabilization and Shiro review.

## ACTIVE CHECKPOINT — distinguish registered asset folders

Goal: resolve [#165](https://github.com/46slv/Nect/issues/165) so same-named registered Asset folders can be distinguished without changing root identity, provenance, placement or the fixed-panel layout.

Current phase: the isolated dot/asset-root-chooser-20261107 branch implements presentation-only root labels and a closed-panel Remove guard. Local acceptance and independent review are complete; exact-candidate CI and normal integration remain pending. The old Codex writer stays stopped, original branches/data are preserved and no installed application is replaced.

Proven: six new root-label cases plus the 14 existing bootstrap cases passed, including Unicode/whitespace ambiguity, pathless browser roots, stable session numbers across Refresh/remount/removal, literal labels and a delayed Remove after Close. The extended catalog browser script passed same-name selection, actual B placement/provenance, one Undo, 20 Refresh cycles, Cancel and explicit Remove. Native IPC stubs passed exact-root reselection and supplied path hints. Both headless modes passed 420x520 chooser bounds; actual screenshots were inspected. Production build and native CSP startup/eval rejection passed. Independent review reported no blocking findings. All 44 related Unit/DOM cases and four additional Asset browser scripts passed, including interrupted bootstrap and stale Composition drag; the full frontend aggregate is running and is not yet claimed successful.

Contract: unique names remain compact; ambiguous catalogs receive per-root session display numbers and existing native paths. Browser labels never infer absolute parent locations. Labels do not enter the registry, authored document or provenance. Existing opaque root IDs continue to select, refresh, remove and place assets.

Next task: commit and non-force push the coherent candidate, create a draft PR, verify remote contents and exact-head Core/Windows CI, then integrate by the ordinary permitted route after checking current ownership, stops and protection rules. Recheck any changed candidate rather than inheriting earlier success.

Approach: one source writer in D:/Documents/Codex/worktrees/Nect-dot-asset-root-chooser-20261107, based on bceac11655846a940fa596a3704181c9fba56d42. The primary production worktree is untouched during implementation. GUI ownership belongs to the concurrent host task; this slice uses Node and sandboxed headless Chrome only. No Nect desktop launch, physical picker or WebView2 interaction is claimed.

Previous accepted evidence: #224 reduced synthetic synchronous 10k cross-Composition Session median from 1609.07 to 1333.92 ms (17.1%); this is not native/frame latency and large scenes remain slow. See desktop/docs/reference-transition-scan-performance.md. Frozen application 66761990 passed a bounded native offline ORBIT Open/color/UndoRedo/SaveAs Cancel/save/exit/new-process reopen/re-edit/SVG/PNG flow, with byte-checked saves and unchanged original input. Its receipt is desktop/test-results/native-artwork-acceptance-20261107/run-0400/result.md. The native processes/windows were released at 04:00:48 UTC. Browser linked/Capture restart evidence is in desktop/docs/color-composition-capture-restart.md. Candidate and post-main CI through #226 succeeded; none of that proves this new candidate's pending CI.

Done for next: save source-bound evidence and normal Git integration for this bounded fix, then choose the highest-value independent accepted unfinished obligation. This is not broad Asset Library or product completion.

State: read Git for the current candidate SHA and dirty status. Source ownership is limited to this chooser slice; PM retains the desktop GUI. The primary branch dot/nect-production-stabilization-20261107 and original stopped branches are preserved.

Residuals: physical/human IME, OS color picker, native dirty-close/save-pending guards, linked/Capture native, malformed/disk-failure paths, full SVG pixel comparison, installer/signing/distribution and Shiro's subjective review remain unverified. Native folder-picker presentation for this fix is also unverified. Final presets, legacy palette migration, broader transfer/ambiguous clipboard provenance, large-scene performance and full REQ-2/3/34/211 remain incomplete. Image Luminance stays internal/unconnected with no public node/full Field-workflow claim. Panels ship fixed; development arrangement work does not authorize user-configurable docking.

Authority: current user takeover/delegation and Mission permit bounded implementation, tests, documentation, coherent commits, non-force development pushes and normal main integration after differences/tests/ownership/stops are checked. No release, force/history rewrite, protection/security/billing changes, PR approval review or safety-denial bypass.

Handoff: CONTINUE_CURRENT_TASK.
