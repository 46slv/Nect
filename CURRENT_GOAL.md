# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: the current user directs Terra to continue development, prioritizing panels and color. Edits, tests, coherent commits and non-force development-branch pushes are authorized. Mainline integration requires its own exact-stack checks and review; release, force/history rewrite, credential or billing changes and bypass of safety denials are excluded. Product decisions remain in [Confirmed requirements](https://app.notion.com/p/01b2ff7802fc410ab4cdb8fd26264495); [REQ-211](https://app.notion.com/p/3fbfd279a6f381a9a7c9c36f5089fba1) governs color and the current [Workspace action](https://app.notion.com/p/40ffd279a6f38132856ce92091df60ea) governs the approved staged panel work. Research plans are not adopted product presets.

Coarse route: panels and color → remaining adopted Session, document, shape and text requirements chosen from live dependencies → measured native stabilization and Shiro review.

## ACTIVE CHECKPOINT — Color Artwork clipboard

Goal: allow graph Copy/Paste within the original Artwork for explicitly typed Color references to another Artwork, while preserving source identity, shared-color semantics and project History.

Current phase: #202–#205 are merged; main reached db02595c. Color Artwork clipboard candidate efc2772b and follow-ups are integrated into the development branch. Independent exact review of 82645262 found no remaining issues; #206 runs applicable CI on that exact head.

Proven: #202 and #204 passed all three applicable checks; #203 and #205 passed their applicable Windows checks. The Color clipboard with follow-up passed 192 focused Node tests, sandbox Chrome Graph Copy/Paste with Undo/Redo and JSON reopen, formatting and build. It covers live palette edits, source expiry, History binding changes, nested draft/gesture and callback reentry, and pending Redo. The synthetic trial receipt is D:/Documents/Codex/2026-11-02/task/key-color-artwork-status-evidence/identity-workflow-20261106/curve-tracking-03/receipt.json; it is not commissioned artwork or native acceptance.

Next task: resolve any adopted exact-review findings, complete applicable CI for this snapshot, and integrate after fresh checks. Continue the highest-value independent eligible work using dedicated fixtures.

Approach: admit only positively parsed typed `colorArtwork` references in the original Artwork. Reject Composition-qualified or opaque references and schema below 64; preserve the source Color and slot identities without creating bindings or packets. Verify owner lineage after callbacks and expire the clipboard when its source disappears or an untrusted project context is rebound.

Done for next: coherent clipboard implementation, focused owner/History and browser tests, independent exact review and applicable CI. Final Mission acceptance still requires real production artwork, native WebView2 exit/restart/re-edit, physical IME and Shiro's subjective review.

State: primary worktree D:/Documents/Codex/2026-11-02/task/Nect-pr178-combination, branch codex/key-color-artwork-status-20261105. Reviewed candidate 82645262 is in D:/Documents/Codex/worktrees/Nect-color-artwork-clipboard-fix-20261107 and #206; read exact local and remote HEAD from Git. The 10,000-point cross-Composition edit remains slow (~1.2–1.6 s); a shortcut was rejected for reentry/metadata hazards. A rare standalone Paste after restoring a Curve without its former point remains safely rejected because its clipboard provenance is ambiguous. Cross-Composition transfer, existing-packet linked import, legacy palette migration, final presets and full REQ-2/3/34/211 acceptance remain incomplete. Native GUI is LOCAL_WAIT while the visible computer-use overlay's ownership is unproven. include/nect/io.hpp:6 writes native 0.77 while docs/model-v0.md:10 says 0.75; the later 0.77 section matches the writer.

Authority: current user instruction and Mission Brief authorize bounded implementation, coherent commit, non-force push and normal merge after applicable checks. Preserve existing user data, single-writer ownership and shared history; no release, force rewrite, incremental billing or safety-denial bypass.

Handoff: CONTINUE_CURRENT_TASK.
