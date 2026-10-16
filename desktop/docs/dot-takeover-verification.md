# dot takeover verification — 2026-10-16 UTC

Base: `462828e614eb86fe85469eb58faadc1f22170d44`. The prior WS writer
released source and foreground before this snapshot was edited. This checkpoint
is development work, not a newly distributed executable.

## Repairs after takeover

- Percentage display is converted before typed-value/drag comparison.
- Non-parameter numeric controls commit their own Session gestures, not a missing
  node parameter; the drag threshold is applied only once. A typed dimension is
  committed before starting its separate drag transaction.
- New virtual Position/Width controls use evaluated defaults without inheriting
  unrelated Anchor/Height expression/source/lock descriptors. Original drivers stay.
- Character shape controls no longer reference an undefined Transform node while
  rendering the palette. This was an actual WIP UI regression.
- Graph/canvas context menus suppress browser navigation/save/print while editable
  text inputs keep their native editing menu. Existing socket disconnect is retained.
- Tests recognize absolute Position movement after opting into new Anchor editing,
  schema 11 support and the newly supported Vector Transform bypass.
- Product-spec entry now distinguishes imported historical text, adopted scope,
  prior native evidence, current WIP and deliberate exclusions. Added source crosswalk.

## Evidence and limits

- Focused transform/anchor/handles/numeric unit tests: initial 17/17 PASS.
- After fixes, Transform affine + numeric DOM + Anchor/handles DOM: 10/10 PASS.
- Character position/shape/typography/kerning/wrap/path DOM regressions: 7/7 PASS.
- Creation/bypass/deletion/ordering/axis focused set: 60/60 PASS.
- Vite Web build PASS. Final source verification uses focused repair tests, not a
  claim that the initial whole-suite run passed.
- Initial broader Node run: 680 reported tests, 663 passed / 17 failed, including
  aggregate failures. Repaired character rendering and stale schema/bypass assertions
  were rerun in the focused sets above. Three DOM files (artwork-multi-move,
  curve-artwork, pen-artwork) still exhaust the Node heap even when sequential at
  1536 MB. Their results are unresolved, not product PASS or confirmed product bugs.
- Strict temporary isolation subsequently passed the Pen 10,000-point case, all
  15 multi-move scenarios, and each of 22 Curve mutation/phase cases. The combined
  Curve child passed with default heap, while repeated cases retained allocations
  under a smaller cap. This narrows the remaining problem to cumulative DOM test
  harness memory; no canonical Session ownership regression was demonstrated.
  Fresh-process scenario isolation now passes at a 768 MB heap cap: Curve 30/30,
  Pen 10/10, multi-move 16/16. Assertions remain; Curve handle editing now focuses
  its anchor first, matching the selected-handle UI. Full aggregate rerun pending.
- Standalone Chromium could not start: local socket operation denied. The permitted
  elevated launch failed during environment setup with a read-only filesystem error;
  no restricted session paths were read or changed. The separate cloud browser could
  not reach the local Vite server (connection refused). No new real-browser result.
- No Windows build, native GUI, physical Japanese IME, or measured AE parity result.
  Prior d6223e9 evidence does not qualify these changed sources.

## Remaining acceptance

The three DOM harness failures are repaired with scenario isolation. Exercise the
approved Transform, dense Remap and Alt-wheel interactions in an actual browser;
verify save/cold reopen/SVG/PNG on the final candidate, then Windows build/native
qualification before trial delivery. Preserve original data and prior ZIPs.
