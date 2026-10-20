# Nect Desktop — production editor

## Mission Brief r1 (adopted 2026-10-16)

Goal: Practical offline Windows editing based on Nect-SVG interaction, incorporating useful Nect capabilities. Shiro reviews operation and helps UI design; AI develops. Preserve the separate legacy C++/Qt Mission and C0.

Design: adopt task-16/astra/brief-r0.md (SHA256 A0D83BD6541092B7220C322291A75CFFF30DB5B45B2861D6B4EB301B7C7366C0) and architecture-addendum.md (C118F137FE34B602477C59D76C263858DCDF4FA66ADB7A390A8C6F5B509F74A5), with the Sol/Astra correction below. Tauri/WebView2 shell; DOM-free JS common command Session for supported Desktop adapters; Rust owns filesystem bindings only. AGENTS fixes common Session commands, not C++ as a language. Read-only Astra consultation /root/session_architecture confirms this decision is within current authority. Imported History/direct UI mutations are migration debt, not proof of the shared Session contract. Legacy formats are distinct and must reject without replacing current work.

Must: coherent Canvas/Graph/Inspector and direct generator edits; practical shapes/Curve/Pen/Boolean/Transform/paint/layers; supported static-font mixed Japanese/English horizontal text with IME; graph identity/fallback and atomic failed edits; safe New/Open/Save/Save As/Close; editable JSON plus SVG/PNG; reproducible offline application and measured responsiveness/startup/memory/size. Preserve imported vertical-text abilities. Advanced typesetting, all font formats, complete legacy conversion, cloud and generic plugins are not added Musts.

Done: actual production artwork → edit → Undo/Redo → save → exit → restart → re-edit → SVG/PNG, adopted Musts verified, major actual-use defects repaired. A first vertical is not product completion. Performance budgets follow measurements. Dashboard: NOT_CONNECTED; this checkpoint is the current reference.

Authority: user's current formal takeover/development instruction. Edits/tests/development branches/commits/non-force push/Draft PR permitted; mainline merge, release, force/history rewrite, credentials/permission/billing changes excluded.

Coarse route: document lifetime → common command Session migration → representative shape/text production and interaction fixes → measured native stabilization / Shiro review.

## ACTIVE CHECKPOINT - REQ303 derived Image Dither

Goal: Add fixed Image-output Ordered/Error Diffusion routes while preserving all existing Shape Dither nodes, saved graphs and source images. Requirement https://app.notion.com/p/3fdfd279a6f3815285a9fefa44391ab6 . Exact user requirement and implementation choices are in desktop/docs/image-dither-output.md.

Current phase: Source implementation, focused verification and separate native candidate compilation complete. Product/source a195cd48d3e0b687ef964c14e4ba5fbe9bcc63c4 is pushed and Draft PR39 is open. Sealing provenance/handoff; UI acceptance remains LOCAL_WAIT.

Proven: Independent task-owned Git object database/worktree fetched directly from GitHub at base 3922a425429d57ce5b8b4e6dba985554f657ff17. Original tested checkout and delivered EXE remain untouched. Existing Keyer demonstrates source-referenced derived Image recipes, deterministic PNG encoding and one-output resolver caching. Locked JavaScript dependencies installed in this worktree. Checkout skills inspected; mission-checkpoint-rollover absent. New fixed Image Dither types deliver recipe-resolved PNGs through Image Output without authored asset growth. Focused regression 117/117 and motif bypass 1/1 PASS; old Shape fixture fingerprints exact. Web build 138 modules, scoped formatting/syntax/diff and native config PASS. Separate custom-protocol release-profile Rust build PASS (3m04s); candidate EXE 9995776 bytes, SHA256 9c625fc2bc2a8592b2410921b642c1e45829aa728bf6899d079094f8a5943d3b, not launched. Full suite NOT_RUN; independent base probe reproduces the unrelated Luminance Instances schema24-vs21 assertion.

Next task: After the parent explicitly releases WS UI ownership, follow desktop/docs/image-dither-handoff.md for the bounded production browser/CSP and isolated native smoke checkpoint. Verify exact source/build/sole-writer state and record TAKEOVER_ACK before a fresh continuation writes or operates UI.

Approach: Follow desktop/docs/image-dither-output.md. Save source plus settings, never mutate imageAssets during evaluation. Preserve existing Shape branches and schema migration. Separate candidate builds. WS UI is exclusively owned by the other tester until parent explicitly releases it; no browser/native UI session is allowed meanwhile. Non-UI tests/builds continue.

Done for next: Deterministic Image consumer data, atomic finite budgets, no authored asset growth, existing Shape fixtures stable, save/reopen and Undo/Redo proven, focused build/check evidence saved; then draft PR with verified remote commit and continuation/native handoff.

State: Draft https://github.com/46slv/Nect/pull/39 targets codex/nect-test-candidate-20261020 at the exact base. Product/source commit a195cd48d3e0b687ef964c14e4ba5fbe9bcc63c4; build-input Git blobs verified against it. branch codex/image-dither-output; base 3922a425429d57ce5b8b4e6dba985554f657ff17; worktree C:/Users/shiro/Documents/Codex/2026-10-21/task-5/Nect-image-dither; Git common directory ../nect-lane.git. Sole source writer in this lane. Original C:/Users/shiro/.codex/worktrees/65e8/Nect is read-only evidence. D:/Documents/Nect is forbidden and its .git link is not followed.

Authority: Current delegated implementation contract authorizes source/docs/tests, isolated build, feature-branch non-force push and draft PR. No main/protected merge, release, force/history rewrite, security changes or Notion edits. No communications to unrelated workers.

Handoff: HANDOFF_READY / LOCAL_WAIT_UI_RELEASE. Source/build/draft-PR delivery is complete; native/browser UI smoke was not attempted because the other lane owns WS UI exclusively. No successor has been started and no second writer exists. A distinct fresh UI continuation is appropriate after trusted release; the complete resume packet is desktop/docs/image-dither-handoff.md. Do not infer release from elapsed time or worker idleness.
