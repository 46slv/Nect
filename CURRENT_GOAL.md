# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: the current user resumed development, prioritizing practical panels and color. Edits, tests, coherent commits, non-force development pushes and normal integration after applicable checks and review are authorized. Release, force/history rewrite, credential or billing changes and bypass of safety denials are excluded. The 2026-11-12 instruction removes backward compatibility with old versions as a goal: do not work solely to preserve old formats or APIs. Protect current authored data, IDs, History and current product defaults. Product decisions remain in [Confirmed requirements](https://app.notion.com/p/01b2ff7802fc410ab4cdb8fd26264495); [REQ-211](https://app.notion.com/p/3fbfd279a6f381a9a7c9c36f5089fba1) governs color and the current [Workspace action](https://app.notion.com/p/40ffd279a6f38132856ce92091df60ea) governs the approved staged panel work. Research plans are not adopted product presets; B remains trial-only when explicitly selected.

Coarse route: panels and color → remaining adopted Session, document, shape and text requirements chosen from live dependencies → measured native stabilization and Shiro review.

## ACTIVE CHECKPOINT — protect current editing and file lifetime

Goal: integrate tested fixes for current editing, import, Viewer and SVG/PNG paths. Preserve current authored data and continue the accepted Mission after each integration.

Current phase: #234 is merged. The remaining reviewed candidates have been integrated into one draft stack, #235 → #236 → #237 → #238 → #239 → #241 → #240, with exact-head Windows CI in progress. #242 is separate and under independent review. Re-read remote main, owner and checks before every integration; confirm the actual merge tree matches the tested candidate before relying on its evidence.

Proven candidates:
- [#234](https://github.com/46slv/Nect/pull/234), head `538cf2b12ce2f71a29d69720066c3a199d6f1c98`, merged as `4228bab183b2aba4bacad7a80d5f26e7acb5d305`: browser reload protects changed inline names and active composition without warning on clean/restored names. Candidate Windows/Core and post-merge Core passed; synthetic IME is not physical acceptance.
- [#235](https://github.com/46slv/Nect/pull/235) `4fedf9195dfff3c8bf005594a15bef189dc712a2` and [#236](https://github.com/46slv/Nect/pull/236) `d2cbb12ed6eae080246e8c8ac5f9e56698fa102a`: protect pending variable-font preparation against file transitions and Layer deletion. Focused Worker/DOM tests, Chrome, build and review passed; Windows CI pending. #235's Core CI was not run: its Core inputs match the successful #234 Core tree.
- [#237](https://github.com/46slv/Nect/pull/237) `a090c6ae8dcf011963b8b0e6d3e2a51c99868f2f`: prevent deletion of a source Layer while a Composition Reference or SVG consumer survives. Integrated stack focused 31/31, Chrome Layers at two viewports and build passed; review found no blocker.
- [#238](https://github.com/46slv/Nect/pull/238) `cd0ebae99197fc254bab2812e10ce957c6f702fa`: retain live-ID Layer Copy within the original Project/Composition/Layer and restore eligibility only on Undo to its original History entry. Integrated stack focused 30/30, real Chrome Copy/deletion and build passed; review found no blocker. #164's copy-time snapshot remains outside this repair.
- [#239](https://github.com/46slv/Nect/pull/239) `3c751e456ac11ec41f866c363d91f25f2f6ae7d2` and [#241](https://github.com/46slv/Nect/pull/241) `6e6d91a86ba440ff976bb3cb34e38fa3be9c0c08`: preserve current Project Import Text-local and Group/Multi Path identities, opaque metadata, and qualified missing references. Integrated Session 33/33, build and review passed; generic Chrome import passed on #239, collision-specific UI was not tested. No old-format-only follow-up is in scope.
- [#240](https://github.com/46slv/Nect/pull/240) `58d38dba7bc59285762ddb5cf498f49c8d11c8fe`: restore default and named Viewer outputs on Composition return. Integrated stack focused 22/22, Chrome Viewer/Layer Copy and build passed; browser registry keeps both additions, 117 scripts.
- [#242](https://github.com/46slv/Nect/pull/242) `ff2112a7b2d311267c7908ce8614dfe751436f08`: allow Nect's bounded solid-sector Angular Gradient pattern in its SVG-to-PNG CLI. Actual CLI color/alpha and nested/mm/precision output plus rejection/no-clobber checks passed; independent safety review and CI are pending.

Next task: finish exact-head required CI and integrate #235 → #236 → #237 → #238 → #239 → #241 → #240 in order, retargeting each to main only after its base merges and verifying the resulting tree and checks. Resolve #242 review and integrate it only on its own proven tree. Avoid expanding Palette presets or coefficients without product authority.

Approach: one writer per scope; independent worktrees remain separate. Keep the established live Layer Copy semantics, Session commands, atomicity, stable IDs and authored data. Preserve existing headless/runtime evidence rather than promoting it to native/physical acceptance. Reconcile each integration's actual tree and checks; repair any new failure before advancing.

Done for next: accepted candidates are integrated with matching required checks and review, or their precise local blocker and resume trigger are recorded. A passing candidate or one artwork vertical is not whole-product completion.

State: as observed 2026-11-12 03:20 UTC, remote main is `4228bab183b2aba4bacad7a80d5f26e7acb5d305`; the draft stack and #242 are unmerged. The prior Desktop primary branch remains clean at `8281ae7ea992b8824e03eeb2b5abe2c66f30008e`; implementation uses separate worktrees. Re-read all heads, CI and ownership because eligible lanes continue independently.

Residuals / LOCAL_WAIT: the prior PM GUI slot is not proven released. Native WebView2 color binding/save/reopen, titlebar Close, physical IME and Shiro's subjective acceptance remain unverified; do not occupy or close another owner's GUI. Current Shared Colors browser evidence is not complete REQ-211 Palette identity/revision acceptance. Final Palette presets/coefficients, broader Field workflow, adopted Musts and production completion remain open. No release, billing, forced history change, safety-denial bypass or other-owner data disposal is authorized.

Authority: the current user instruction, Mission Brief and repository instructions authorize these bounded fixes, tests, coherent commits, non-force pushes and normal checked integration. The 2026-11-12 compatibility instruction supersedes old-format/old-API preservation as a goal, without authorizing loss of current authored data or changing current product defaults.

Handoff: CONTINUE_CURRENT_TASK.
