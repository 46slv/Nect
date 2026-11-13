# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: the current user resumed development, prioritizing practical panels and color. Edits, tests, coherent commits, non-force development pushes and normal integration after applicable checks and review are authorized. Release, force/history rewrite, credential or billing changes and bypass of safety denials are excluded. The 2026-11-12 instruction removes backward compatibility with old versions as a goal: do not work solely to preserve old formats or APIs. Protect current authored data, IDs, History and current product defaults. Product decisions remain in [Confirmed requirements](https://app.notion.com/p/01b2ff7802fc410ab4cdb8fd26264495); [REQ-211](https://app.notion.com/p/3fbfd279a6f381a9a7c9c36f5089fba1) governs color and the current [Workspace action](https://app.notion.com/p/40ffd279a6f38132856ce92091df60ea) governs the approved staged panel work. Research plans are not adopted product presets; B remains trial-only when explicitly selected.

Coarse route: panels and color → remaining adopted Session, document, shape and text requirements chosen from live dependencies → measured native stabilization and Shiro review.

## ACTIVE CHECKPOINT — Node Foundation migration

Goal: replace remaining legacy built-in node declarations and evaluator dispatch with trusted Node Foundation bindings while preserving current authored documents, stable IDs, Session commands, Undo/Redo, pixels and product defaults. The user requested that this task work alone; earlier tasks remain stopped.

Current phase: resumed on 2026-11-13. Keyer PR #301 was merged at `7267d4e9328bee8ecc6e37eed6bbcf3a99f7e06b` after all exact-head CI passed, the existing independent review found no issues, and current-main integration tree `e99ff13ad41bda6f553bdf4f56c41173ccf233c2` passed 346 focused tests, build and Chrome Keyer/Color checks. The next bounded candidate migrates the private Image Output endpoint.

Proven: the Image Output candidate preserves the definitions/catalog projection hash `801e189b0021426e8fd778b311e8bb5c817b74bb1d439bb5b8d179533693ff32`, existing Keyer RGBA and bypass behavior. Its 31 focused files / 197 tests, build, format, production CSP and headless Chrome image import, pixel, history and SVG/PNG flows passed. Native WebView2 and physical input are not claimed.

Next task: publish the Image Output candidate, check exact-head CI, compare its prospective merge tree against the live main and integrate only after applicable checks and review. Continue with the next eligible existing node in this same task when safe.

Approach: one writer in isolated D: worktrees; preserve existing node semantics and unsupported behavior. Test the real Session/evaluator and production renderer paths. Do not restart earlier tasks or perform work solely for old-version compatibility.

Done for next: coherent commit and non-force push, required checks and applicable review, matching current-main integration evidence, and a post-merge tree readback.

State: base `7267d4e9328bee8ecc6e37eed6bbcf3a99f7e06b`; candidate branch `codex/node-foundation-image-output-20261113`. Existing image and Color worktrees and the unrelated running development server are not owned by this task. The prior stopped checkpoint is preserved at `codex/mission-checkpoint-20261112` / `0f777b890646c99d91e2104ab2df778b86878ba5`.

Authority: the user's 2026-11-13 instruction resumes this task alone and authorizes scoped node migration and ordinary validated Git integration. Preserve the earlier automatic-review refusal for #296 and the Field denial; do not retry either through another route. Release, added billing, force/shared-history rewrite and unrelated data changes remain outside scope.

Handoff: CONTINUE_CURRENT_TASK.
