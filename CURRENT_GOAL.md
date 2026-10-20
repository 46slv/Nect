# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - Layer name drag ordering repair (2026-10-20)

Goal: Fix the reported inability to reorder Layers in the delivered corrected aca9f590 Windows trial. Limit changes to Layer-list input, visual insertion mapping and the shared reorder command; preserve stable records, selection, scene/export order and one Undo per move.

Current phase: Source repair, focused regression and frontend build are complete. Implementation cae664a60ca827d90583d2e1bedad178bc9870a6 is pushed normally and matches the remote branch and open Draft PR #35. Native packaging remains a local environment blocker, not native acceptance.

Proven: On base acca2616, synthetic DOM reproduction fails for Layer-name drags and reversed-list edge placement. The name button was excluded by the old pointerdown guard; target IDs were passed directly to the authored back-to-front array. The repair accepts name/padding drags, previews above/below slots, captures only after movement, and dispatches the existing reorder-layer command with an explicit null frontmost boundary. Production npm run build PASS (137 modules), scoped Prettier and diff checks PASS. 18/18 focused tests passed across layer-reorder-dom, artwork-ordering and artwork-ordering-dom; exact records, JSON reload, SVG paint order, one Undo/Redo, no-op Redo retention, plain click and interruption/stale/lock safety are covered. Actual browser/native input remains NOT_RUN.

Next task: On a provisioned Windows build host, run the existing supported frontend/Rust custom-protocol route from the exact candidate, hash the distinct artifact, then request checklist03 native retest. Do not silently replace the aca9f590 executable.

Approach: Keep input state in desktop/src/layer-reorder.js and commit through common Session. No HTML native drag/drop, OS clipboard or shell changes. Canvas and SVG already consume authored layer order; no renderer rewrite is needed.

Done for next: Reviewable source/test/docs change, focused checks and frontend build, coherent non-force remote checkpoint/Draft PR; exact Windows-artifact limitation and targeted human retest documented.

State: codex/nect-layer-reorder-fix-20261020, product source cae664a60ca827d90583d2e1bedad178bc9870a6, base acca2616ab38c703bcf3e4b0ef465eeae7c62ffc. Draft https://github.com/46slv/Nect/pull/35 targets codex/nect-regression-validation-v2; closeout metadata does not change app inputs. Isolated D:/Documents/Nect-worktrees/codex-task5-layer-reorder-20261020; sole writer assigned by parent. C: was full, so dependencies/build/temp/cache use task-owned D: paths. Existing check-windows-toolchain reports cargo/rustup/rustc absent, SDK26100 false, MSVC unresolved, WebView2 present; no native build/launch attempted. No unrelated checkout or runtime modified.

Authority: Current finite bug delegation and user's authorized Nect edit/test/commit/development push/Draft PR scope. Parent owns Notion and actual user delivery. No main merge, release, force/history rewrite, credentials/permissions/billing, denied server/browser retry, desktop focus or unrelated local Codex task operation.

Handoff: LOCAL_WAIT for Windows artifact/native retest. Source fix is synchronized for review; parent owns delivery and next build-host routing. No additional unrelated implementation is authorized by this finite checkpoint. Broader Nect Mission and native acceptance remain open.
