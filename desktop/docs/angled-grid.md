# Angled editor grid

Bounded brief, based on PR63 `49283bca1c90e83aec3bb8c901017d854e526dc3`.
The current editor has nine relative grid origins and an axis-aligned pattern;
there is no editor-grid angle. This is the parent's selected production feature,
not authorization inferred from a Notion Candidate.

Grid **Angle °** stores finite degrees from−360 through360, default0. Authored
values are retained; evaluation normalizes to one turn (−180,180], with exact
cardinal bases. Zero/±360 use the existing pattern and per-world-axis snap path.
Missing/invalid resolver input falls back to0; invalid authored values reject
atomically. Nonzero authored angles require schema49 (including full turns).
Old missing/zero-angle documents retain their incoming schema/data on load.

The shared basis is `world = origin + R(angle) × grid-local`, using the chosen
canvas-relative origin. Snapping subtracts origin, applies the inverse rotation,
uses the existing spacing/Math.round convention, then rotates the correction
back. Each free grid-local axis has the existing6 CSS-pixel threshold. Bounds
use their small Cartesian anchor set; candidates prefer two eligible axes, then
the smallest rigid correction. All selected geometry receives one translation.

Existing explicit guide/character/artboard/object targets are searched first
with their existing priorities. When any wins, the angled-grid fallback does
not override it. Constrained drags intersect a grid line with the allowed world
ray and enforce the screen-distance threshold; feedback uses the actual clipped
rotated line. The old world-axis solver is not reinterpreted as grid-local.
One-axis-only sources such as guide drags do not acquire an invented second
coordinate; they retain explicit targets, with rotated-grid fallback out of scope.
Unpaired Cartesian sets over256 candidates are also excluded from that fallback.

Display inverse-transforms the four canvas/view corners to bound line indices,
then clips finite segments to the canvas/view rectangle. At most2048 segments
are generated. Dense grids draw a deterministic lattice subset and show
**Display every N lines**; snapping still uses the authored world spacing.
Geometry/markup cache uses the canvas/view bounds and basis settings. Pan/zoom
refresh only the visible grid overlay through the existing canvas view owner;
ordinary pointermove snapping performs no view-layout read. Zoom/pan never author
different spacing or change the lattice phase.

Only the artboard editor grid rotates. Artwork, absolute guides, ruler/world
coordinates and SVG/PNG artwork exports do not. Angle uses existing numeric
Session gestures: one commit/Undo, no-op for unchanged value, neutral cancellation.
No object-local grid, per-selection origin, ruler rotation or general snap-engine
redesign. Preserve PR63's Escape repair, the Fireworks Ring output and old candidates.

Acceptance:0/45/−30/90, nine origins and resize/fractional dimensions, negative
coordinates/ties/thresholds, finite clipping/density cap, explicit target priority,
constraints, legacy/default/invalid state, Undo/Redo/JSON and artwork invariance.
Use a few native CDP synthetic placement cases and the existing PNG export path;
physical/OS input, native save dialogs and clean cold reopen remain unverified.
