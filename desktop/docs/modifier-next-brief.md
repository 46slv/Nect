# Next modifier brief

**Status:** The grid-origin slice below is implemented and verified in schema 15. Variable-width open-path strokes remain a proposal only; they are not implemented or approved by this brief.

## Current baseline

`docs/modifiers.md` catalogs 23 modifier additions, including tangent-aligned Instance on Points and three luminance-to-geometry effects. Instance on Points uses authored anchors, so Resample Path is already the documented route to more/evenly spaced targets. Halftone, Ordered Dither, and Line Screen share the same bounded field sampler and mask clipping; they accept solid/two-stop gradient vector fills, while images remain unsupported. Each effect now exposes a finite grid origin, default `(0, 0)`, without moving source geometry.

Relevant seams: `src/luminance-field.js` samples a fill and accepts an `{x, y}` origin; `src/luminance-marks.js` makes dots, Bayer squares, or horizontal/vertical stripes; `src/luminance-mask.js` clips marks to source geometry. `src/core.js` maps the authored `Vector2` origin to the existing sampler. `Stroke` stores one width, and `Expand Stroke` converts its painted input using that width through `src/expand-stroke-geometry.js`.

## 1. Implemented: expose luminance grid origin

**User benefit:** Move the texture phase relative to artwork and align multiple effects without translating source geometry. This adds practical control to the shared luminance mapping while reusing its existing sampler API.

**Implemented contract:** Halftone, Ordered Dither, and Line Screen have a bounded `Vector2` `gridOrigin`, default `[0, 0]`, with each finite component within ±100000 px. In document coordinates Y increases downward, so the origin is the top-left corner of grid cell `(0, 0)`. The value maps to the sampler's `{x, y}` origin and shifts cell-center sampling / mark phase without changing source geometry. Existing fill sampling, darkness/invert meaning, fixed Bayer matrix, per-cell mark shape, compound-mask clipping and work caps are preserved. No grid rotation, image inputs, new field/value type, new mark mode or budget increase was added.

**Implementation and migration:** `src/core.js` adds the parameter, resolved-value validation, schema-15 backfill and existing bounded transactional preflight. `src/main.js` labels the control with the cell corner and Y-down convention. The schema-14 default fingerprint remains unchanged; non-default origins participate in output identity so downstream retained edits invalidate when the phase moves. Focused core and DOM tests cover the slice. The [12-layer grid-origin playground](../examples/luminance-grid-origin.json) compares 0, quarter-cell and half-cell origins on unchanged sources; it was serialized/reopened and its actual SVG/PNG reviewed.

**Focused acceptance evidence:**

- Old schema-14 documents containing each effect backfill `[0, 0]`, evaluate identically, and save as schema 15; explicit values round-trip.
- A shifted origin moves sample centers / marks for all modes while source geometry stays fixed; the example visually compares three phases.
- Negative-index cells, edge clipping, compound holes, empty geometry and all three modes are covered.
- Non-finite/out-of-range direct or driven origins reject transactionally. Existing work-budget rollback, bypass, lock, Undo/Redo and serialization paths are covered by focused core/DOM tests.

**Resolved risks:** Origins are bounded at ±100000, mask clipping still uses source/cell geometry in document space, Bayer ranks still use integer grid indices, and default schema-14 output remains unchanged. Existing 4096-cell / geometry / mask budgets remain unchanged.

## 2. Proposal: variable-width open-path strokes

**User benefit:** Tapered line art and calligraphic strokes without manually outlining the centerline. This is a practical Illustrator-style gap: Adobe documents its Width tool for variable-width strokes and reusable width profiles ([official guide](https://helpx.adobe.com/illustrator/using/tool-techniques/width-tool.html)). Nect already has stroke expansion, so this can build on a familiar workflow rather than introducing a general deformation system.

**Possible bounded version:** Extend Expand Stroke with start/end width multipliers, both defaulting to `1`, and a linear ramp by normalized arc length on open contours. Keep the current constant-width path unchanged at the defaults. Initially exclude editable mid-path width points, saved profiles, closed contours, and parity claims. A changed profile on unsupported contour input should fail clearly. Treat this as a proposal until its visual contract and bounds are accepted; nonuniform offsets need more than forwarding the current constant-width value.

**Dependency and ownership:** One writer owns the Expand Stroke settings/migration, `src/expand-stroke-geometry.js`, and focused geometry/core/DOM tests; no concurrent writer on that helper. Confirm how adaptive flattening, joins, caps, zero-width tips, multi-contour inputs, and output budgets behave before expanding scope. Do not implement by changing Closed Path Offset semantics or adding a new field type.

**Focused acceptance:** Uniform defaults preserve current outputs; a straight open line and cubic produce the expected taper at each end; reversing the path reverses the profile; equivalent curves with different upstream resampling retain a stable arc-length-based ramp; caps/joins and degenerate/cusp/over-budget inputs are covered, with unsupported cases rejected transactionally.

## Explicitly deferred

Do not bundle rotated luminance grids, raster-image tracing/sampling, arbitrary per-anchor width editing, or a generic reusable Field node into either slice. Those broaden geometry, persistence, or sampling contracts and need their own demonstrated consumer need. The origin-control slice is implemented; reassess the stroke proposal independently after trying it with representative drawings.
