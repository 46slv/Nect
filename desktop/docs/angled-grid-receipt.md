# Angled editor grid verification

Base: PR63 `49283bca1c90e83aec3bb8c901017d854e526dc3`, branch
`codex/guides-dom-panel-maintenance`. Product/build source:
`32e863925b7b4d73ecd388dc226aa995b230652b`. This adds 312 lines and removes
25 across `guides.js`, `direction-snapping.js`, `core.js` and `main.js`.
No equivalent editor-grid angle existed at the base. The bounded design and
delivered limits are in [angled-grid.md](angled-grid.md).

## Observed checks

- 42 unique focused checks passed: 34 model and 8 DOM/axis checks, zero failures
  or skips. The final fractional-angle DOM check was rerun after `step="any"`;
  that rerun is not counted as another unique check. Commands and output are in
  `test-results/angled-grid/{model,dom,dom-final}.log`.
- Model coverage includes 0/45/-30/90 degrees, all nine origins, resize and
  fractional dimensions, negative coordinates and rounding ties, thresholds at
  multiple zooms, finite clipping, the 2048-line cap, explicit-target priorities,
  constrained movement, atomic invalid/stale rejection, Undo/Redo, JSON and
  unchanged artwork/SVG. Zero/full-turn/fallback snap results were also compared
  directly with PR63's exact `guides.js`, rather than a rewritten approximation.
- DOM checks include fractional typing, preview/commit, focus, neutral Escape
  and pointer cancellation (including late pointerup), one Undo/Redo, rejected
  361 degrees, persistence, dense-grid indication, and nearby guide/axis behavior.
- Frontend build and locked, offline native custom-protocol build passed. The
  frontend retains its existing Vite chunk-size warning. Changed-file formatting,
  JavaScript syntax and native-build configuration checks passed.
- Native Tauri/WebView2 CDP synthetic interaction passed two configurations on
  SHIRO-WS: 45 degrees/top-left at 100% with pan, and -30 degrees/center at 120%
  with pan. The fixture is 600 x 600, three nodes/two connections, one rectangle,
  no images/dither. Each case verified a known grid intersection, both actual
  rotated feedback lines, one movement Undo/Redo, exact Escape cancellation and
  JSON reload. Settings alone preserved artwork/SVG. Angle scrub cancellation
  also restored exact document/file/history/selection state. No page errors.
  Screenshots were captured after reload returned the view to Fit; the 100%/120%
  assertions and placement precede those screenshots. Both saved screenshots
  were visually inspected for the finite rotated lattice and retained artwork.
- Existing PNG export UI in production-CSP headless Chrome produced identical
  600 x 600 PNG files at 0 and 45 degrees. Their decoded RGBA also matches the
  corresponding SVG rasterization. PNG SHA256:
  `bca2b28cb1e5320d933866becaa705d456f4a0e82232765542d6b62b43742063`.
  RGBA SHA256:
  `c6038ca9d932ba1ab8c7515bfd7ffe452821dacec590d5b55347052e8db4430b`.

The native harness initially targeted a stroke-only move path; selecting the
actual move icon fixed the harness. The PNG harness initially left the Export
menu open; closing it before Grid and asserting the actual angle fixed its
preparation. Failed preparation logs/fixtures remain in the evidence. Neither
required a product-code change after the build-source commit.

## Candidate and preservation

Native executable: 10,005,504 bytes, SHA256
`68e8750b437522b7273bcae300bdccb7af0071fb06a1533aa60d279be4c6bf0d`.
The dedicated runtime uses its own executable-adjacent acceptance profile.
Its fixture was persisted through the test CLI and the process exited through
the test close route without forced termination. This does not establish an OS
save/open/clean-exit workflow.

All 21 prior executable hashes and all 69 files in the latest sealed PR63 and
Fireworks Ring outputs still match. The sample and earlier candidates were not
overwritten. The final candidate copies the tested executable, bundle, logs,
fixtures, screenshots and this receipt; its external manifest records each file.

## Limits and remaining verification

Authored finite degrees range from -360 through 360, including fractions;
evaluation uses (-180, 180]. Zero/full turns retain the old grid path. Missing or
invalid resolver values fall back to zero; invalid authored changes reject.
Nonzero authored angles require schema49; old missing/zero documents retain their
incoming schema on load.

Explicit snap targets keep priority. One-axis-only sources such as guide drags
and Cartesian source sets exceeding 256 pairs do not use rotated-grid fallback.
Dense grids display a labeled deterministic subset, while snapping retains the
authored spacing. These are intentional bounded limits, not general solver
support. Grid settings do not transform artwork, absolute guides or rulers.

Physical/OS input, native OS Save/Open/PNG dialogs and normal clean cold reopen
remain unverified because the available native route is CDP synthetic input.
The Chrome PNG route is separate from native OS export. No general performance
claim, unrelated audit, main merge, release or Library upload was performed.
