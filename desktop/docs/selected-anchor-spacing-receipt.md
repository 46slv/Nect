# Numeric anchor spacing verification

Base: PR67 `ade3cd40521c0b632e7ff3b251a070df41c25903`,
`codex/selected-anchor-distribution`. Product/build source:
`c4128b578770942a52d0a75531e5cb735d97de11` (initial implementation
`5188fbfd8ed8bf8621a7104813c28fccb73d49ea`, followed by the input-retention fix).
The parent-selected slice and coordinate-unit decision are in the
[brief](selected-anchor-spacing.md). No schema addition.

Set X/Y spacing places2-10000 selected anchors at minimum + index \* interval
in artboard document/viewBox units. Minimum stays fixed; maximum moves. Source
traversal resolves ties, independent of selection order. Zero collapses only the
chosen axis. Existing fixed-end Distribute remains available and distinct.

## Verification

- 93 focused checks passed, zero failures/skips, in the final ten-file run:
  `point-spacing`, `point-spacing-dom`, `point-distribution`,
  `point-distribution-dom`, `point-alignment`, `point-alignment-dom`,
  `selected-point-delta`, `selected-point-delta-dom`,
  `batch-node-point-commands`, `canvas-gesture-session`.
  Evidence: `evidence/focused-fixed.log`.
- New interval coverage:10 model checks and8 DOM checks. Two/257 points,
  fractional/zero/negative/nonfinite/overflow values, source-order ties,
  compound holes/units/storage order, transformed routes, unchanged minimum/
  orthogonal coordinate/handles/IDs/unselected data, sparse Remap/source
  preservation, Undo/Redo/no-op/Redo retention and JSON/SVG roundtrip pass.
  Whole-selection lock/drive/connection/singular/unsupported/stale guards pass.
- Input/change/Enter alone do not commit. Escape discards the draft. Both panel
  copies synchronize staged values. Successful apply preserves the entered
  interval with refreshed identity tokens; repeated apply is a no-op.
- Native Tauri/WebView2 CDP synthetic passed one fractional Y interval8.125 on
  a transformed Path Remap at120%, pan[-20,-70], rotation35 degrees, nonuniform
  scales1.5/0.7. Canvas600x500, five nodes/four connections, no images/dither.
  Visible/unobscured clicks, typing without document/history mutation, expected
  artboard positions, unchanged other-axis/unselected geometry/source, selection/
  active row and view matrix, one Undo/Redo, repeat no-op and JSON/SVG reload
  passed. No page errors. The screenshot was visually inspected.
  Success evidence: `evidence/native-fixed/receipt.json` and its JSON/SVG/PNG.

Initial DOM preparation re-Shift-clicked points that Undo had already restored,
thereby toggling them off. The test now asserts the restored selection directly;
no assertion was weakened. Initial native repeat exposed a product issue: render
reset the interval to10. The follow-up commit retains the successfully applied
value in refreshed controls, and the DOM assertion checks it without retyping.
The initial logs/fixture remain saved separately; they are not success evidence.

## D output and candidate

Final frontend build passed in4.38s (existing chunk-size warning); locked offline
release/custom-protocol Cargo build passed in41.51s using the existing D target.
Runtime, new acceptance profile, child TEMP/TMP, logs and candidate use
`D:\Nect-codex-output-20261022-task13-f336c94f2b714cd5bc73d6ca646c8f2e`.
The unchanged frontendDist, checkout/dependencies and Cargo home remain on C.
No global setting/guard changes, deletion, failed save or capacity hold occurred.

Candidate EXE:10,008,576 bytes, SHA256
`94451b65faa4a85aa6e78bdacf5ba5174a9a91712ee15cde89311b1a40665f44`.
Destination: `D:\Nect-codex-output-20261022-task13-f336c94f2b714cd5bc73d6ca646c8f2e\anchor-spacing-artifact-c4128b5`.
The external manifest records actual package hashes/final branch identity.
All25 prior EXEs and268 sealed files match before and after build/native checks.
Both dedicated native processes exited through the normal test close route with
their fixtures preserved; no forced termination or ordinary AppData/Recovery use.
C free942,747,648 bytes and D free37,556,428,800 bytes after checks.

Physical/OS input, native Save/Open dialogs, normal OS cold reopen and installer
remain unverified. JSON reload is the synthetic loader route. No main merge,
release, Library upload, generic expressions, multisource work or further point
features. The requested point-operation slice is closed at this bounded scope.
