# Selected anchor alignment verification

Base: PR65 `b918852cd558e4cb46521b3809237b39c3c9a33b`, branch
`codex/direct-anchor-marquee`. Product/build source:
`1a120b8600969b3af17e37e8e569826a337a709f`.
Product delta: `core.js`, `main.js`, `point-alignment.js`, 250 additions/nine
removals. No persistent model or schema addition. The new parent-selected slice
and completion conditions are in [selected-anchor-alignment.md](selected-anchor-alignment.md).

## Verified

- 72 focused checks passed, zero failures/skips: 46 model and 26 DOM checks.
  Model command covers `point-alignment`, `selected-point-delta`,
  `batch-node-point-commands`, `canvas-gesture-session`, `direct-selection`.
  DOM command covers `point-alignment-dom`, `selected-point-delta-dom`,
  `direct-anchor-marquee-dom`. Final output is in
  `test-results/point-alignment/{model,dom-final}.log`.
- All six min/center/max operations use artboard-space selected anchor centers.
  X[0,10,30] centers at15. Two-anchor minimum, rotation/nonuniform scale,
  unchanged unselected data/relative handles/IDs/contour membership, sparse Remap
  entries, one Undo/Redo, repeat no-op/Redo retention and JSON/SVG roundtrip pass.
  A zero-motion legacy Remap entry stays byte-for-byte represented as before;
  retained handle data is unchanged and suspended edits are never rebound.
- Locked/driven/connected members, locked/unsupported/singular routes, missing or
  duplicate IDs, malformed deltas, stale source/route/fingerprint/revision/Session
  reject atomically. Selection snapshots guard both panel copies; another active
  gesture blocks the operation. Current Remap point controls are in Graph;
  owned-point controls remain in Graph and Inspector.
- Frontend build passed in4.32s, with the existing Vite chunk-size warning.
  Locked offline native custom-protocol build passed in42.05s. Scoped formatting,
  syntax, diff whitespace and native-build configuration checks passed.
- Actual native Tauri/WebView2 CDP synthetic interaction passed two configurations
  on SHIRO-WS, 600x500 canvas: Curve X-center at100%, pan[18,-12], six nodes/four
  connections; Remap Y-center at120%, pan[-20,-70], five nodes/four connections.
  Both use rotation35 degrees, width scale1.5 and height scale0.7, without images
  or dither. Pointer targets were checked as visible/unobscured before clicks.
  Expected range midpoint, orthogonal/unselected positions, relative handles,
  selection/focus/rows, exact view matrix and inline size, one Undo/Redo, repeat
  no-op and JSON/SVG reload all pass. No page errors. Both saved screenshots were
  visually inspected. Evidence: `native-verified/receipt.json` and its JSON/SVG/PNG
  files; these are the successful native results.

Native preparation was corrected without product changes: the legacy compound
fixture had no `editor` object, so it now uses the existing guide-settings
resolver; the initial Remap viewport placed an anchor under a toolbar, so its pan
was changed and an occlusion assertion added. Neither is counted as a product
regression. The source/build inputs remain identical to the product commit.

## Capacity event and candidate

C: reached zero free bytes during a native evidence retry. npm/log/screenshot and
failure-fixture writes returned ENOSPC; zero-byte PNGs in `native-final` are failed
outputs, not verification. They remain in the workspace and are excluded from
the success evidence package. Tests were retried to a fresh `native-verified`
directory only after checking at least64MiB of free reserve.

Only25 regenerable `.pdb` files in this task's verified
`native-target/release/deps` cache were removed, reclaiming84,127,744 bytes. No
candidate, source, user file, other task file or recovery data was removed.
`cache-cleanup.json` records exact paths/sizes. No whole-volume investigation or
additional cleanup was performed after the parent's capacity notice. Both build
stages had already succeeded before the event; no rebuild was attempted after it.

The standalone executable was already saved in the dedicated runtime directory:
10,007,552 bytes, SHA256
`9358a2d9344e2a8d1333d11dfdb45e88a5f1cfa2f1a16ea1a676753af041cd35`.
Packaging is bounded by a fresh exact-size/free-space check with64MiB reserve;
the external candidate/capacity record reports its actual completion status.
Failed writes are not treated as successful saves. The dedicated process exited
without forced termination after preserving its project fixture through the CLI
test route. That route does not establish native OS Save/Open.

All23 prior EXE hashes and185 sealed files in the latest PR63/64/65 and Fireworks
outputs still match. Physical/OS input, native Save/Open dialogs and normal clean
cold reopen remain unverified. Host free space remains low and needs the parent's
separate capacity work before subsequent large builds/copies. No main merge,
release or Library upload.
