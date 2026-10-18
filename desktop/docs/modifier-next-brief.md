# Next modifier brief

**Status:** Grid Origin is implemented and verified in schema 15. A bounded start/end width-multiplier slice for Expand Stroke is implemented and verified in schema 16; editable middle width points and saved profiles remain a proposal.

## Current baseline

`docs/modifiers.md` catalogs 25 modifier additions, including tangent-aligned Instance on Points and four luminance-to-geometry effects. Instance on Points uses authored anchors, so Resample Path is already the documented route to more/evenly spaced targets. Halftone, Ordered Dither, Line Screen, and Error Diffusion Dither share the same bounded field sampler and mask clipping; they accept solid/two-stop gradient vector fills and supported embedded RGB/RGBA PNG images (the image-input slice landed in schema 17). Error Diffusion Dither is a distinct schema-19 node; its deterministic forward raster diffusion leaves the Bayer mode unchanged. Each effect now exposes a finite grid origin, default `(0, 0)`, without moving source geometry.

A fifth bounded luminance consumer, [Luminance Instances](luminance-instances.md), now maps shared sampled darkness to motif scale and angle in schema 20. It returns unpainted cubic Paths and selects centers rather than clipping motifs; existing four mark effects remain unchanged.

Relevant seams: `src/luminance-field.js` samples a fill and accepts an `{x, y}` origin; `src/luminance-marks.js` makes dots, Bayer squares, or horizontal/vertical stripes; `src/luminance-mask.js` clips marks to source geometry. `src/core.js` maps the authored `Vector2` origin to the existing sampler. `Stroke` retains one base width; `Expand Stroke` now applies bounded endpoint multipliers in `src/expand-stroke-geometry.js`.

## 1. Implemented: expose luminance grid origin

**User benefit:** Move the texture phase relative to artwork and align multiple effects without translating source geometry. This adds practical control to the shared luminance mapping while reusing its existing sampler API.

**Historical schema-15 slice contract:** Halftone, Ordered Dither, and Line Screen have a bounded `Vector2` `gridOrigin`, default `[0, 0]`, with each finite component within ±100000 px. In document coordinates Y increases downward, so the origin is the top-left corner of grid cell `(0, 0)`. The value maps to the sampler's `{x, y}` origin and shifts cell-center sampling / mark phase without changing source geometry. Existing fill sampling, darkness/invert meaning, fixed Bayer matrix, per-cell mark shape, compound-mask clipping and work caps are preserved. No grid rotation, image inputs, new field/value type, new mark mode or budget increase was added.

**Implementation and migration:** `src/core.js` adds the parameter, resolved-value validation, schema-15 backfill and existing bounded transactional preflight. `src/main.js` labels the control with the cell corner and Y-down convention. The schema-14 default fingerprint remains unchanged; non-default origins participate in output identity so downstream retained edits invalidate when the phase moves. Focused core and DOM tests cover the slice. The [12-layer grid-origin playground](../examples/luminance-grid-origin.json) compares 0, quarter-cell and half-cell origins on unchanged sources; it was serialized/reopened and its actual SVG/PNG reviewed.

**Focused acceptance evidence:**

- Old schema-14 documents containing each effect backfill `[0, 0]`, evaluate identically, and save as schema 15; explicit values round-trip.
- A shifted origin moves sample centers / marks for all modes while source geometry stays fixed; the example visually compares three phases.
- Negative-index cells, edge clipping, compound holes, empty geometry and all three modes are covered.
- Non-finite/out-of-range direct or driven origins reject transactionally. Existing work-budget rollback, bypass, lock, Undo/Redo and serialization paths are covered by focused core/DOM tests.

**Resolved risks:** Origins are bounded at ±100000, mask clipping still uses source/cell geometry in document space, Bayer ranks still use integer grid indices, and default schema-14 output remains unchanged. Existing 4096-cell / geometry / mask budgets remain unchanged.

## 2. Implemented: bounded variable-width Expand Stroke

**User benefit:** Tapered line art and calligraphic strokes without manually outlining the centerline. This is a practical Illustrator-style gap: Adobe documents its Width tool for variable-width strokes and reusable width profiles ([official guide](https://helpx.adobe.com/illustrator/using/tool-techniques/width-tool.html)). Nect already has stroke expansion, so this can build on a familiar workflow rather than introducing a general deformation system.

**Implemented contract:** Expand Stroke has Start / End Width Multiplier parameters, both defaulting to `1` and bounded from `0` to `4`. Unequal values ramp linearly per open contour by normalized flattened-centerline arc length. Equal values reuse constant-width expansion, including for closed contours; unequal values explicitly reject any closed contour. A zero endpoint collapses that tip independent of cap, and both zero multipliers produce empty output. Existing centerline flattening, joins/caps, output budgets, solid-stroke requirement and type-conversion bypass rules remain in force. This is a bounded endpoint ramp, not arbitrary per-point width editing or Illustrator numeric parity.

**Implementation and migration:** `src/core.js` adds the two scalar settings, validates direct and evaluated values, bumps the project schema to 16, and backfills `1` / `1` for older Expand Stroke nodes. `src/main.js` exposes bounded, lockable Inspector and graph fields. Defaults preserve the previous geometry fingerprint, and existing Path Remap edits remain retained. Closed Path Offset semantics and the Stroke paint descriptor are unchanged.

**Focused acceptance:** Geometry tests cover the default/constant fast path, open-path start/end ramps, endpoint tips and empty output, closed-contour rejection, caps/joins and bounded failure cases. Core/Session tests cover schema-15 backfill with matching output fingerprint and retained remap, settings-sensitive output, Undo/Redo, JSON round-trip, invalid fallback/expression/source-driven values, locks and atomic rollback. DOM coverage verifies both bounded controls. The [12-layer playground](../examples/variable-stroke-playground.json) compares uniform, start-tip and end-tip cubic strokes and was serialized/reopened and visually reviewed. Focused results and one Web build are recorded in `docs/editing-refinement.md`; they do not establish native GUI interaction or full Illustrator parity.

## 3. Proposal: advanced width editing

Editable middle width points, reusable profiles, profile handles, closed-contour variation, and parity claims remain unimplemented proposals. Reassess them independently after trying the endpoint-ramp slice on representative drawings. Do not add a generic Field node or change Stroke's base-width semantics as part of this bounded feature.

## Explicitly deferred

Do not bundle rotated luminance grids, raster-image tracing/sampling, arbitrary per-anchor width editing, or a generic reusable Field node into either slice. Those broaden geometry, persistence, or sampling contracts and need their own demonstrated consumer need. The origin-control slice is implemented; reassess the stroke proposal independently after trying it with representative drawings.
