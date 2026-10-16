# Nect desktop redesign

## Mission Brief

Goal: a coherent Windows desktop editor rebuilt from the Nect / Nect-SVG behavior prototypes. AI implements/builds/verifies; the user reviews UI and operation. First deliverable is a real executable with a minimal editing vertical.

Authority: direct 2026-10-16 08:55 / 09:01 UTC user steering and delegated UI brief. Edits, local validation, commits, development push and draft PR are authorized. No protected-main merge, release, force/history rewrite, OS/account/permission/billing change, unknown install, or Nect-Cloud mutation.

Isolation: C:/Users/shiro/Documents/Codex/2026-10-16/task-12/nect-desktop, independent clone, branch codex/desktop-workspace-20261016. Sole source writer takeover from 08de238a85a545ec81c9668007334dd1cf050e0f, verified against remote branch and draft PR23 before editing. The task-11 checkout is read-only to this task; its review application PID84564 stays open. Serialize builds with Local\NectBuild (length 15), zero timeout, nonabandoned, release/dispose, /m:2 /nodeReuse:false.

Done: blank -> rectangle drag -> Objects/canvas/Inspector selection -> move -> fill/W/H -> Undo/Redo -> native save/reopen/edit. Windows executable, meaningful local test, size/scale renders, explicit migration limits and durable Git checkpoint.

Coarse route: native editing vertical -> user review/bounded repair -> next real editing/graph capability on the same Document/Session. No full feature migration or general graph engine in the first vertical.

## Active checkpoint

Goal: close the two observed first-screen layout issues: external fixed footer with Fit/manual viewport behavior; 240 DIP Inspector below 800 logical while retaining 280 DIP at wider widths.
Current phase: both local layout repairs implemented and verified; development checkpoint synchronization.
Proven: Windows Release build PASS, 26 layout-only Qt-event checks PASS, final pipeline/683-logical/wide widget images visually inspected. Footer is external at22 DIP; Fit/manual mapping and Inspector240/280 boundaries are verified without authored/History/save-state changes. First vertical's 23 semantic checks at08de238 are reused. See docs/desktop-workspace/verification.md.
Next task: user UI/interaction review of the corrected binary; incorporate further feedback only through another bounded packet.
Approach: reuse the 22 DIP status bar, opt into Canvas Fit tracking on resize, preserve manual view mapping, keep Inspector numeric columns/height/type. No authored model, History or persistence changes.
Done for next: focused checks and build pass, three target images inspected, exact local/remote/PR HEAD equal.
State: TAKEOVER_ACK from verified08de238. task-12 owns the repair checkout; final exact local/remote/PR HEAD is reported in the completion receipt. Builds and offscreen validation finished; mutex released. Task-11 and existing apps/projects are untouched, review PID84564 responding.
Authority: delegated 2026-10-16 two-issue repair. No new features/full rewrite, main merge, release or force operations.
Local wait: native screenshot/recording/physical acceptance. Fresh native inventory failed MXC HRESULT0x80070003; no same-path retry without new host evidence. Qt renders and semantic workflow continue. This is not a build blocker.
Handoff: bounded repair complete; source writer releases when the final local/remote/PR checkpoint is synchronized. No successor writer was created.
