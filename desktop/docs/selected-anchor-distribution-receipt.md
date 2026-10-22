# Selected anchor even-spacing verification

Base: PR66 `3b04ffb967c86ece95c412d91622fc1a0f22262f`, branch
`codex/selected-anchor-alignment`. Product/build source:
`32edebe64a0712003317803d73735775718e4e61`. New parent-selected slice:
[brief](selected-anchor-distribution.md). No persisted schema addition.

Horizontal/Vertical controls space 3-10000 selected anchors in one editable
source by artboard X/Y, with exact endpoints fixed. Exact coordinate ties use
resolved source traversal, independent of click order or global point storage.
PR66's inverse transform, identity guard, precision and atomic point command are
reused. One changed operation is one Undo; no-op retains dirty/history/Redo.

## Checks

- 91 distinct focused checks passed, zero failures/skips: the ten-file focused
  run passed90; the expanded distribution model file then passed12 including one
  additional compound-contour case. Other tests were not rerun for that test-only
  addition. New distribution coverage is12 model checks and7 DOM checks.
- The focused run covers `point-distribution`, `point-alignment`,
  `selected-point-delta`, `batch-node-point-commands`, `canvas-gesture-session`,
  `direct-selection`, and DOM suites for distribution/alignment/delta/Direct
  marquee. Logs: `evidence/focused.log` and `evidence/model-final.log`.
- Three/257 points, negative values, tied/reversed selection, unchanged endpoint
  authored values, orthogonal coordinate/relative handles/unselected data/IDs,
  compound holes/units/contour and storage order, rotated nonuniform routes,
  sparse Remap preservation, JSON/SVG reload, one Undo/Redo and repeat no-op pass.
- Locked/driven/connected members (including stationary endpoints), unsupported/
  singular/locked routes, missing/duplicate IDs and stale source/Remap fingerprint/
  revision/Session reject atomically. DOM checks cover minimum count, both panel
  copies, selection/focus history, held controls and conflicting gestures.
- Native Tauri/WebView2 CDP synthetic interaction passed2 cases on SHIRO-WS:
  transformed Curve horizontal spacing at100%, pan[18,-12], six nodes/four
  connections; transformed Path Remap vertical spacing at120%, pan[-20,-70],
  five nodes/four connections. Both use35-degree rotation, width scale1.5,
  height scale0.7, 600x500 canvas, no images/dither. Visible/unobscured anchor
  clicks, expected coordinates, handles/unselected data, selection/active row,
  unchanged view matrix, one Undo/Redo, repeat no-op and JSON/SVG reload pass.
  No page errors; both PNG screenshots were visually inspected. Evidence:
  `evidence/native/receipt.json` and its JSON/SVG/PNG files.

## First D build and saved candidate

This was the first actual D Cargo build and EXE execution, beyond the prior small
readback probe. Frontend build succeeded in4.41s (existing chunk-size warning).
Locked offline release/custom-protocol native build succeeded in2m09s with
`--target-dir D:\Nect-codex-output-20261022-task13-f336c94f2b714cd5bc73d6ca646c8f2e\cargo-target`.
Child TEMP/TMP, build logs, native runtime, fresh acceptance profile and test
output all used the dedicated D root. No global settings or storage guard changed.

The unchanged `frontendDist=../dist` still uses the small C frontend output;
source/dependencies/Cargo home also remain on C. C free bytes were866,537,472
before build and844,484,608 after native checks; D free bytes were37,617,745,920
after checks. No capacity hold, failed save, deletion or cache cleanup occurred.

EXE:10,008,576 bytes, SHA256
`f7f9e3173c160642a4fb0307c11bbf15d431cf5ac3043a954df1d48b4e5ed2d5`.
Dedicated candidate destination:
`D:\Nect-codex-output-20261022-task13-f336c94f2b714cd5bc73d6ca646c8f2e\anchor-distribution-artifact-32edebe`.
The external manifest/receipt records packaging hashes and final branch identity.
The dedicated native process PID77400 exited via its normal test close route
after preserving its fixture; CDP9477 is no longer listening. User instances and
ordinary AppData/Recovery were not used. All24 prior EXE hashes and234 sealed
files matched both before and after build/native verification.

## Remaining coverage

Physical/OS input, native Save/Open dialogs and normal OS cold reopen remain
unverified; JSON reload is a synthetic loader check. No installer, main merge,
release or Library upload was performed. No new image/dither changes or unrelated
full audit. Optional future work: explicitly entered point spacing, if separately
selected; current controls distribute only within the existing endpoint range.
