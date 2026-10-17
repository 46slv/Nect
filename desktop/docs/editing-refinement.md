# Editing refinement — active specification

## User outcome

Slant editable text without rotating it, select dense path points, zoom naturally,
and adjust every editable numeric control consistently. Existing artwork must keep
its appearance and authored graph connections when opened.

## Approved scope

- Extend the existing Transform: Anchor XY, Position XY, uniform/independent width
  and height scale (100% UI), Skew, Skew Axis and Rotation. Preserve legacy relative
  Offset and scalar scale semantics. Do not claim exact AE signed-matrix parity
  without a measured comparison. Opacity/shutter/motion blur are outside this slice.
- Use the same affine for path geometry, character provenance, inline caret,
  alignment and SVG/PNG export; retain editable text and stable identities.
- Path Remap controls use small screen-space markers with separate hit regions.
  Dense points remain distinguishable; show relevant handles for selection/hover.
  Zoom/pan and downstream transforms must not enlarge markers into overlapping discs.
- Alt + wheel zooms the artboard around the pointer, also while editing Remap.
  Preserve existing pan and numeric editing; never zoom the application/browser page.
- Every editable numeric field supports drag and typing. Preview immediately,
  commit one drag as one Undo, cancel cleanly, respect ranges, locks and drivers.
  Text, selectors and toggles retain their appropriate interaction.
- Suppress browser navigation/Save/Print context menus on graph/canvas surfaces.
  Retain existing Nect socket actions and native text-input editing menus.

## Acceptance

Representative transformed horizontal/vertical text, legacy JSON/connections,
independent scales, dense Remap drag/cancel/Undo, cursor-centred zoom, typed percent
followed by drag, local preference fields, document/guide numeric commands, context
menu routing, save/reload and SVG/PNG. Focus tests on these changes; preserve producer
boundaries for browser, native Windows and physical Japanese IME evidence.

## Current evidence and next work

The initial implementation was delivered as development trial 7496aee on 2026-10-17
07:28 JST. Offline Windows release build succeeded; the application was not launched
for native acceptance in that verification task. Browser tests cover selected
Transform handles, numeric gestures, Remap vertex operations and document guards.
They do not establish physical IME, exact AE skew parity, Alt-wheel interaction feel
or all tangent-handle affordances. These remain explicit acceptance obligations.

On 2026-10-17 09:16 JST Shiro reopened independent dot-side fixes and Brief work.
Inspect every editable numeric control category against drag/typing/Undo/cancel
semantics and repair demonstrated gaps. Do not wait for subjective trial feedback
before such independent repairs. Do not infer acceptance of deferred legacy features.

A practical production checkpoint is editable horizontal/vertical text plus vector
artwork, with transform/paint/alignment, save/cold reopen/re-edit and SVG/PNG export.
Use a representative fixture and source-bound evidence; do not rebuild a new mock UI.
Full Illustrator parity, legacy Qt format conversion and generic integrations remain
outside the adopted first-delivery scope.

## Numeric preference fix — 2026-10-17

A locked active composition previously prevented all numeric dragging, including
Alignment Spacing and layout drafts that remain editable by typing. Pointerdown,
move and pointerup now distinguish local preferences from authored parameters.
Local preference drags remain available; graph/text/command targets still honor
composition locks. Cancel restores the preference and does not alter document
history. The regression first failed (12 remained 12 rather than 32), then passed
after the fix; numeric DOM and pure-unit tests pass 4/4. This is source-test evidence,
not a new native trial qualification.

## Remap hover lifetime fix — 2026-10-17

Hover-only tangent handles previously remained visible after pointer movement left
Canvas. Outside-stage pointer movement now clears hover while preserving clicked
point focus and active gestures. A focused DOM regression reproduced two stale
handles, then passed after the fix; clicked-point handles remain visible. The same
view-navigation test retains Alt-wheel and view-only/document-preservation checks.
This is DOM evidence, not native GUI or physical-input acceptance. The distributed
trial remains 7496aee; subsequent source fixes have not been packaged into a new ZIP.

## Stroke presets — 2026-10-17

The requested dotted/dashed appearance already existed in Stroke's dash pattern,
cap and SVG/PNG path. A named Solid / Dashed / Dotted selector now exposes it in
both graph and inspector. Dashed sets 12 px / 6 px with butt caps; Dotted sets
0 px / 8 px with round caps. Solid clears the dash pattern, retaining the cap.
Manual patterns remain editable and display Custom. Each preset is a single
undoable node-settings command and respects composition locks. Focused stroke
DOM/core tests pass 4/4; no new Windows ZIP has been produced. Pucker/Bloat meaning
is awaiting clarification; no deformation node has been added in this slice.

## Non-destructive modifier expansion (2026-10-17)

Shiro requested more Illustrator-like editing including Mirror, then explicitly authorized
repeating research and additions with Blender modifiers as inspiration. There is no fixed
modifier-count ceiling. Each addition must have an actual vector-art use, searchable naming,
editable parameters, bypass, atomic common commands, focused verification and bounded output.
This does not expand merge/release or account permissions.

### Closed Path Offset — explicit group inputs

- A Path with explicit contour-group tags is partitioned by the existing group identity rule:
  equal tags, including untagged and empty-tag contours, remain together as one region instance.
  Each group is passed independently through the existing closed nonzero-region kernel.
- Each group retains its own holes, winding, splits/erosion and internal topology rejection.
  Separate groups are not unioned and are not rejected for overlap or contact with each other.
- Paths with no explicit group tags still make exactly one existing kernel call. This includes
  Text's ungrouped CompoundPath: it remains one global nonzero region collection rather than
  being split into glyphs. A zero amount preserves the exact upstream geometry and identities.
- The 32-contour / 256-authored-point caps are pre-summed for the full source. Flattening,
  subdivision, topology checks, estimated join work and output-vertex limits are consumed from
  one shared budget across groups. Group count therefore does not multiply editor limits; if any
  shared budget is exhausted, later groups reject before processing, even if one might erode empty.
- Derived contours receive deterministic per-group IDs/tags. The modifier remains unstable for
  downstream Path Remap, and Text character provenance is routed once from the full source.
- Evidence: grouped Offset core tests plus the focused Offset kernel/pipeline/DOM and Join set
  pass54/54. The Web build is checked after this change set; no full-suite or native GUI claim.

### Mirror

- Geometry node before Fill / Stroke, found through Add node → Mirror.
- Left-right: reflect about the vertical line through Axis Center. Up-down: reflect about
  the horizontal line. Angle: reflect about a line through Axis Center, clockwise degrees
  from horizontal (0 horizontal, 90 vertical). Axis Center initially [480,270]; it is explicit,
  not a continuously inferred artwork bounding center.
- Keep Original off replaces the evaluated output with its reflection; on retains source
  plus reflected output as a live pair. Upstream edits update both; the upstream document
  remains editable. Coordinates and angle support the existing numeric drag/typing UI.
- Relative Bézier handles are reflected with the linear matrix. Each copy has independent
  paint groups while hole contours stay together. This is no seam weld or Boolean merge.
- Deterministic generated IDs; unstable fingerprint protects downstream remaps from silent
  retargeting. Direct canvas character editing through Mirror is suspended; edit source Text.
  Raster Image mirroring and artwork-selection flip shortcuts are separate future capabilities.
- Focused evidence: Mirror core 4/4, DOM 1/1, Join regression 5/5, Web build pass. No claim of
  native Windows interaction, PNG visual review or full Illustrator equivalence.

Next: review existing Repeater/Offset/Boolean/Remap coverage, then add missing useful geometry
modifiers in coherent slices instead of duplicating nodes or inventing a generic framework.

### Corner Round

- Non-destructive straight-edge polygon/polyline fillet, inspired by the non-destructive
  edge treatment of [Blender Bevel](https://docs.blender.org/manual/en/4.4/modeling/modifiers/generate/bevel.html).
  This is a 2D design adaptation, not Blender feature parity.
- Path input, Radius 0–100000 document px (default 12), standard bypass. Positive radius
  requires zero Bézier handles; curves fail explicitly. Radius 0 preserves any input geometry.
- Circular tangent arcs approximated by cubic segments of at most 90 degrees. Adjacent
  half-edge limits reduce the effective radius on short edges. Open endpoints remain fixed;
  concave signed turns, holes/group membership and winding order are retained.
- Zero-length edges at rounded corners and 180-degree reversals fail explicitly. This does
  not weld, repair self-intersections or promise global topology preservation.
- Geometry output bounded to 10000 contours /100000 points. Generated IDs and fingerprint
  follow the existing remap suspension contract. Edit the upstream polygon/curve as the source.
- Evidence: geometry 8/8, Session graph/history 2/2, DOM 1/1 and Mirror regression 4/4 passed;
  Web build passed. A combined rounded-star + live-Mirror SVG exported successfully. Raster
  preview was not inspected (cloud SVG rasterizer unavailable); no native GUI claim.

### Continuing research sequence

- Resample Path: count-controlled or maximum-spacing equally spaced samples along each contour,
  as a foundation for deliberate point-density editing and later displacement. Curves become
  polylines; name and docs make approximation explicit. Reference:
  [Blender Resample Curve](https://docs.blender.org/manual/en/5.2/modeling/geometry_nodes/curve/operations/resample_curve.html).
- Simplify Path: error-tolerance-driven point reduction with explicit topology checks;
  [Blender Decimate](https://docs.blender.org/manual/en/4.4/modeling/modifiers/generate/decimate.html)
  is inspiration, not an implementation contract.
- Expand Stroke: geometry outlines from width/caps/joins, then Boolean-editable shapes;
  [Blender Solidify](https://docs.blender.org/manual/en/5.3/modeling/modifiers/generate/solidify.html)
  motivates converting thickness into geometry. Need a bounded open/closed offset contract.

These are an evolving implementation sequence under Shiro's explicit research/addition
instruction, not a reason to rebuild existing Repeater/Offset/Mirror or run full audits.

### Resample Path

- Provides Count (2–1024 points per contour; closed contours require at least 3), default 32,
  and Maximum spacing (0.0001–100000 document px). Spacing chooses whole equal arc-length
  intervals per contour: for open contours, `N = max(2, ceil(L / spacing) + 1)`; for closed
  contours, `N = max(3, ceil(L / spacing))`. The actual gap is no larger than requested, but
  generally differs because only whole segments are emitted. This is intentionally not
  Blender's approximate Length mode contract.
- Samples at equal intervals of the source's numerically evaluated arc length using the
  existing Paper.js geometry engine; output handles are zero. Open endpoints are retained;
  closed paths avoid duplicating the seam. Zero-length nonempty contours use their open/closed
  minimum sample count and remain finite. Spacing mode activates schemaVersion 12; older count
  documents load through a one-way migration that normalizes Resample's historical Boolean
  operation and backfills the dormant spacing default.
- This creates a polyline approximation. Low counts can remove corners/small features or
  alter shape topology; it is not tolerance-preserving Simplify. Source remains editable.
- Preserves groups and closed state, namespaces deterministic generated IDs, and uses the
  existing unstable fingerprint/remap protection. Output limits 10000 contours /100000 planned
  points plus a 2,000,000 source-points × planned samples work estimate across contours; spacing
  rejects a contour requiring more than 1024 output points before allocating its output.
- The common bounded-modifier preflight now includes Mirror, Corner Round and Resample
  alongside Offset. Invalid newly introduced geometry errors roll back commands; missing
  wiring stays editable and pre-existing errors can be repaired.
- Evidence: Resample core11/11 + DOM1/1 + parameter-validation1/1 passed; Web build passed
  (Vite 77 modules, with the existing large-chunk advisory). Earlier Corner Round Session3/3 and
  Offset pipeline12/12 checks passed. Native GUI and raster visual review remain open.
  Distributed EXE unchanged.

Next: seeded Roughen for controlled irregular outlines, with Resample before it for
predictable point density. Inspiration: [Blender Grease Pencil Noise](https://docs.blender.org/manual/en/3.3/grease_pencil/modifiers/deform/noise.html).
Use amount in px, deterministic seed and an open-endpoint preservation toggle. No animation,
opacity noise or unqualified topology guarantees in this first slice.

### Roughen

- Static point-position noise inspired by the cited Grease Pencil Noise modifier. Amount
  0–100000 px bounds each X/Y component (default 3); integer Seed 0–2147483647 (default 1).
  Keep Endpoints defaults on and pins only open-path ends.
- Uses deterministic integer hashing of seed plus source contour/point IDs. Re-evaluation,
  save/reopen and the same source IDs keep the noise pattern. No clock/random global state.
- Does not add points; place Resample Path first to control density. Relative Bézier handles
  remain unchanged, translating with their point. Large noise may self-intersect; no cleanup.
- Preserves contour groups/closed flags and source editability. Geometry bounded at 10000
  contours /100000 points with finite values. Standard bypass, common history and locks.
- Evidence: helper6/6, graph+Session2/2, DOM1/1, Resample regression5/5, Web build pass.
  Actual native interaction and raster preview remain unverified.

Compatibility: new node names require this development build. The previously distributed
7496aee trial does not recognize these new modifiers; do not open their project data there.
Next slice: Smooth Path, a bounded position-only neighbor-average filter with repeat count
and pinned open endpoints, following [Blender Smooth](https://docs.blender.org/manual/en/latest/modeling/modifiers/deform/smooth.html).
It intentionally may shrink geometry; it will not claim topology preservation or curve fitting.

### Smooth Path

- Non-destructive point-position neighbor averaging, inspired by the cited Blender Smooth
  modifier. Factor 0–1 (default 0.5; drag step 0.01), Iterations 1–50 (default 3), Keep
  Endpoints on by default. Closed contours wrap their neighbors; unpinned open ends use
  their sole neighbor. Each pass reads a simultaneous prior-position snapshot (Jacobi).
- No added points. Relative Bézier handles, contour grouping and closed state remain;
  output gets deterministic modifier IDs. Factor 0 is an independent geometry clone.
- This can shrink/collapse silhouettes. It is not Bézier curve fitting or topology repair.
  A useful stack is Resample Path → Roughen → Smooth Path → Fill / Stroke.
- Bounds: 10000 contours,100000 points,2,000,000 point-iteration operations; finite results
  and stable convex interpolation even for large finite coordinates.
- Focused evidence: helper6/6, graph+Session2/2, DOM1/1 and Roughen graph regression2/2;
  Web build passed. A worker accidentally invoked a broader test command; its final outcome
  was not captured, so it is not counted as an aggregate pass. Future helper lanes must run
  only their exact assigned test file; no npm test or full-suite command.

### Trim Path

- Path → Path modifier inspired by [Blender Trim Curve](https://docs.blender.org/manual/en/4.5/modeling/geometry_nodes/curve/operations/trim_curve.html).
  Start and End are per-contour arc-length fractions from 0 to 1 (defaults 0 and 1),
  and Start may not exceed End. A zero-width interval returns empty geometry.
- Cubic Bézier slices retain their cubic geometry and relative handles; arc-length lookup is
  numerical. Full 0–1 ranges preserve the source closure and geometry. Partial selections are
  open contours, including partial selections from closed sources. The closed-path seam is the
  fixed start/end boundary: there is no implicit wraparound. Edit upstream Text for direct
  character editing.
- Each contour is trimmed independently. Contour groups remain intact. Zero-length contours
  remain finite and are emitted open for non-empty partial intervals. Modifier IDs are generated
  deterministically from the node and output order; its unstable fingerprint suspends downstream
  Remap edits when the retained geometry changes. The source graph stays editable.
- Bounded to 10,000 contours and 100,000 input/output points, with finite lengths, positions and
  handles. Bypass, common history, parameter locks, and transactional range validation apply.
- Focused evidence: Trim Path core/DOM 11/11 plus disconnected modifier fallback validation 1/1 and Web build pass. Removed unreachable fallback-range checks from the paint evaluator and placed them in document validation, so disconnected invalid modifier values also reject atomically. No claim of native GUI review
  or formal Blender feature parity.

Continue researching and adding useful geometry modifiers in coherent, focused slices. Finish
concrete slices with focused verification, not merely a list of modifier ideas.

### Expand Stroke and editable modifier playground

- Converts the existing Stroke's PaintedPath to unpainted Path geometry for Boolean use;
  width/cap/join/miter settings are read from Stroke. Existing fill, stroke color and alpha
  do not paint the result. Add Fill after conversion. Bypass is disabled because the types differ.
- Solid input accepts straight edges and bounded cubic centerlines; cubic segments are flattened
  adaptively to a fixed 0.1 px centerline tolerance. The endpoint cap direction follows the first
  or last cubic tangent as represented by the flattened polyline, including square-cap extension.
  This is polygonal approximation: the flatten tolerance does not bound total outline error after
  joins, offset construction, integer cleanup and output-grid rounding. All-zero dash patterns
  count as solid; any nonzero pattern rejects. Open caps support butt, square and round. Closed
  contours use Clipper's
  [EndType.Joined](https://angusj.com/clipper2/Docs/Units/Clipper/Types/EndType.htm) for centered
  stroke bands with holes, not Polygon inflation. Earlier open-only research is superseded by
  concrete Joined rectangle/reversed-winding probes and tests.
- Independent source groups are processed independently; overlapping outlines within a group
  are unioned by the pinned Clipper kernel. Width 0 is empty; nonzero width below 0.0002 px
  rejects. Miter limit 1–100. Input32 contours/256 authored points; flattened centerlines are
  limited to 256 vertices, 4096 subdivision nodes and depth24. Output2048 points; round-work4096.
- Local integer grid 0.0001px; round-arc tolerance at most0.1px. Conservative coordinate/width
  envelope prevents unsafe integer/world restoration. No exact SVG-stroke equivalence claim.
- Evidence: focused geometry/core/DOM tests and Web build cover straight and curved caps, curved
  closed-band hole winding, groups, flattening / output budgets, Boolean input, unsupported dash
  input and transactional rollback. No exact SVG-stroke equivalence or global final-boundary
  error guarantee.
- `scripts/modifier-playground.js` builds `examples/modifier-playground.json` (18 editable
  layers, all evaluated after serialize/reopen) and a seven-modifier SVG preview. Fixed IDs
  make repeated JSON/SVG generation byte-identical. The SVG was rasterized with PyMuPDF and
  visually inspected; all seven examples and labels are present without clipping. This is a
  shared-core export review, not native application GUI acceptance.

See [modifier catalog](modifiers.md) for current controls, recipes and limitations.

### Simplify Path and Dash Path

- Simplify Path uses bounded iterative RDP on straight-edge contours with tolerance0–100000px
  (default1). It preserves open endpoints, contour order/groups and a closed ring's winding;
  if reduction collapses or reverses a valid ring it retains the original ring. Zero tolerance
  clones geometry; positive tolerance requires zero Bézier handles (Resample first). It can
  change small features/intersections and is not a topology-preserving cleanup. Input10000
  contours/100000points; maximum2,000,000 distance checks. Inspiration:
  [Blender adaptive Simplify](https://docs.blender.org/manual/en/4.3/grease_pencil/modifiers/generate/simplify.html).
- Dash Path generates actual retained cubic pieces using arc length: Dash Length0.0001–100000px,
  Gap0–100000px, Offset±100000px. Positive offset advances the periodic pattern; negative wraps.
  Gap0 preserves the source. Closed-seam fragments from the same dash are joined into one open
  piece. This is geometry before Stroke, unlike the existing paint-only dash style. Visual dots
  still use Stroke's Dotted preset; dash geometry requires positive lengths. Inspiration:
  [Blender Dot Dash](https://docs.blender.org/manual/en/3.6/grease_pencil/modifiers/generate/dash.html),
  whose point-count controls differ from Nect's distance controls. Bounds128 input contours,
  2048 input points,512 dash intervals,200000 point×interval work,100000 output points.
- Focused evidence: Simplify helper10 + Session2 + DOM1; Dash interval planner5 + graph/Session3
  - DOM1, total22/22, Web build pass. Common bypass/history/locks and editable JSON/SVG apply.
- Modifier Playground now contains nine examples in20 evaluated editable layers. The actual SVG
  raster was inspected for labels, clipping and shape output. Native application acceptance remains
  unverified. The earlier seven-example proof sent in chat is preserved as that historical image.

Subdivide Path is the next completed addition below: it adds Bézier anchors without converting the
curve to a polyline. See its exact per-edge contract, bounds and focused evidence.

### Subdivide Path

- Path → Path, with **Segments per Edge** 1–32 (default 2). Each existing open or closed edge is
  split into that many equal-parameter Bézier portions using Paper.js `Curve.getPart(t0, t1)`;
  this is not arc-length Resample. Geometry and tangent continuity are preserved. Segments=1 is a
  geometry-preserving clone with fresh deterministic modifier IDs.
- Open endpoints and their unused relative handles remain intact. Closed contours keep the original
  seam and do not duplicate the closing endpoint. Contour groups remain unchanged. An edge whose
  endpoint handles are both zero retains zero handles on its line portions for polyline-only
  downstream nodes. Empty and one-point contours stay finite.
- Preflight bounds: 10,000 input contours, 100,000 input points and 100,000 estimated output
  points. The estimate is checked before constructing Paper geometry or output points; non-finite
  source/results and non-integer/out-of-range evaluated Segments values fail without partial edits.
  Fallback values are checked during document validation as well. Output has deterministic IDs and
  an unstable fingerprint; direct canvas character editing is disabled through the modifier while
  upstream Text remains editable.
- Inspired by [Blender Subdivide Curve](https://docs.blender.org/manual/nl/4.5/modeling/geometry_nodes/curve/operations/subdivide_curve.html), which adds control points while retaining Bézier/poly shape. Nect's control is **segments per edge** (the number of resulting pieces); Blender exposes **Cuts**, so the numeric setting is intentionally different.
- Focused evidence: the two assigned Subdivide Path test files pass 11/11; `npm run build` passes.
  No native Windows GUI review is claimed. Build emitted Vite's existing warning that the minified
  main chunk exceeds 500 kB.

### Pucker & Bloat

- Selected under Shiro's broad repeated-modifier-addition instruction. The earlier ambiguous
  word “パンク” has not been separately confirmed; this does not invent an answer to that question.
- Inspired by [Adobe Pucker & Bloat's described direction](https://helpx.adobe.com/illustrator/using/reshaping-objects-effects.html),
  with an explicit Nect cubic-control contract, not claimed numerical parity: for t=Amount/100,
  anchors scale by (1−t) around Center and absolute Bézier controls by (1+t). Relative handles
  are recomputed from those results. Positive bloats, negative puckers; zero clones geometry.
- Amount −100..100% (default0). Automatic Center uses the bounds of all source anchors, not
  exact curve extrema or a separate center for each group. Disable it for a manual Vector2 Center.
  Straight polygons intentionally gain curved handles. Large amounts can fold/collapse geometry.
- Finite values,10000 contours/100000 points; deterministic modifier IDs, original groups/order,
  common bypass/history/locks, generated fingerprint and explicit upstream Text editing.
- Focused evidence: helper9 + graph/Session2 + DOM1, plus Subdivide core/DOM11, total23/23;
  Web build pass. Playground now has 26 evaluated editable layers showing11 modifiers and both
  positive/negative Pucker & Bloat variants; actual exported SVG raster visually inspected.
  Native Windows GUI and exact Illustrator numerical matching remain unverified.

### Wave Points

- A static sine displacement of existing Path anchors, inspired by the effect category in the
  [Blender Wave modifier manual](https://docs.blender.org/manual/en/5.0/modeling/modifiers/deform/wave.html).
  Nect's explicit contract is its own; Blender feature or numeric parity is not claimed.
- Direction **Vertical** offsets Y using each anchor's X coordinate; **Horizontal** offsets X
  using Y. Displacement is `Amplitude × sin(2π × coordinate / Wavelength + Phase)`. Amplitude is
  −100000..100000 px (default20), Wavelength 0.0001..100000 px (default150), and Phase −360..360°
  (default0). The sine angle is `2π × coordinate / Wavelength + Phase × π/180`; phase is reduced
  modulo360° and spatial turns modulo one before sine evaluation.
- Only existing anchor positions move. Relative Bézier handles remain unchanged and move with
  their anchor; curved segments between anchors are not continuously warped into an exact sine.
  No points are added. Resample Path first to choose finer wave detail. **Keep Endpoints** defaults
  off; when enabled it pins the first and last points of each open contour, and has no endpoint
  effect on closed contours.
- Input is bounded to10000 contours and100000 points, so evaluation is a single bounded pass.
  Every source position and relative handle must be finite. The relevant coordinate divided by
  Wavelength must be within ±1e12; generated coordinates and arithmetic must remain finite, or the
  operation fails. This protects precision and avoids silently accepting invalid geometry.
- For nonzero displacement, generated contour/point IDs are deterministic for modifier output
  order; zero amplitude returns a cloned copy with the source IDs. The output is marked unstable
  so a changed result suspends downstream Remap edits. Use the normal Path→Path bypass, common
  history and lock behavior. Canvas character editing through Wave Points is unavailable; edit
  upstream Text instead.
- Focused evidence: 17/17 checks pass (Wave helper/core/DOM, disconnected-modifier validation,
  and Pucker core); this includes a geometry-precision failure rollback regression. Wave Points
  and Pucker & Bloat are included in bounded modifier preflight. The Web build passes with the
  existing large-chunk warning. This is source-level evidence, not native GUI acceptance.
- The editable playground now replaces only the negative Pucker sample with Line → Resample Path
  (49 points) → Wave Points → Stroke; the positive Bloat sample remains. The shared-core generator
  validates each serialized/reopened layer and writes the SVG export. This is not a native GUI
  acceptance claim.

### Reverse Path

- Path → Path modifier inspired by [Blender Reverse Curve](https://docs.blender.org/manual/en/5.4/modeling/geometry_nodes/curve/operations/reverse_curve.html).
  It has one Path input and no numeric controls. Open contours reverse point order; closed contours
  retain their original first anchor as the seam and reverse the remaining points. Swapping each
  point's relative Bézier handles preserves the exact cubic segments and closed silhouette while
  reversing traversal direction; contour groups and metadata remain intact.
- Preflight bounds are 10,000 contours and 100,000 points. Generated contour / point IDs are
  deterministic from the node and output order. The output carries an unstable fingerprint so
  downstream Path Remap edits suspend when the reversed geometry changes. Canvas character edits
  through Reverse Path are disabled; edit the upstream Text source. Standard Path bypass, Session
  history, input locks and JSON/SVG export apply.
- Reverse Path → Trim Path changes which half a 0–0.5 selection follows. Reverse Path before
  Dash Path similarly changes the pattern's traversal order; closed paths retain their existing
  seam anchor.
- Focused evidence: helper + Reverse Path core/DOM checks pass 11/11; the Web build passes with
  the existing large-chunk warning. No native GUI or full-suite claim.

### Set Path Start

- Path → Path modifier with one Start scalar from 0 to 1. Each nondegenerate closed contour moves
  its seam by that fraction of its own arc length, retaining the forward winding and exact cubic
  geometry. An interior seam splits the intersected Bézier and rejoins the tail to the head as one
  closed contour; a source-vertex seam rotates anchors without introducing a duplicate knot.
- Open contours pass through unchanged. Start 0 and 1 are geometry identities (with derived IDs),
  while empty and zero-length closed contours stay closed and finite. Contour order, groups and
  metadata are retained; output IDs are deterministic and the result has an unstable fingerprint.
  Canvas character editing through Set Path Start is suspended at the upstream Text source.
- Preflight validates finite vectors, Start range and the 10,000-contour / 100,000-point bounds.
  The node participates in transactional modifier preflight, Path bypass, Session history and input
  locks, plus JSON roundtrip and SVG export. Numeric drag is bounded 0–1 in 0.01 steps.
- Useful stack: Set Path Start → Trim Path / Dash Path to select a different seam-relative beginning
  while preserving forward direction. This describes Nect's seam-control contract and makes no
  claim of Adobe or Blender numeric parity.
- Focused evidence: Set Path Start core tests 8/8, DOM test 1/1, disconnected modifier parameter
  validation 1/1, and Web build pass with the existing large-chunk warning. No native GUI or
  full-suite claim.

The catalog lists 16 modifiers; the editable playground covers 14 in 35 layers. The final row compares the same half-Trim with unchanged, reversed and shifted-start traversal; the exported SVG was visually inspected.
Continue with useful researched slices rather than duplicating existing nodes. Native trial
packaging remains a separate dependency.

### Taper

- Inspired by [Blender Simple Deform Taper](https://docs.blender.org/manual/en/latest/modeling/modifiers/deform/simple_deform.html), with an explicit static 2D Nect contract, not numerical parity.
- Path → Path. Amount −50..50% (default0); Horizontal/Vertical axis (default Horizontal).
  One shared bounding box includes anchors and absolute Bézier control endpoints across all contours.
  Along the chosen axis, normalize to u in [−1,1]; scale the perpendicular coordinate about the
  shared midpoint by 1 + Amount/100 × u. Positive grows toward the maximum-axis end.
- Maps anchors and absolute controls, then recomputes relative handles. This is a control-point
  approximation to nonlinear deformation of the curve interior; use Resample Path first for
  finer geometry. It does not add points. Control-point scale factors remain 0.5–1.5.
- Empty/zero-span/zero-amount inputs independently clone geometry. Groups, order and closure
  are retained. Finite arithmetic and 10000-contour/100000-point budgets are checked. Standard
  deterministic generated IDs, unstable fingerprint, bypass, Session history/locks, numeric drag,
  transactional preflight, and upstream Text editing restrictions apply.
- Evidence: helper10 + graph/Session2 + DOM1 + disconnected-validation1 =14 focused checks pass;
  Web build passes with the existing large-chunk warning. No native GUI/full-suite claim.
- Catalog now16 modifiers. The existing 35-layer playground covers14; Taper's focused rectangle
  evidence explicitly checks output anchors, JSON roundtrip and SVG output.

### Bend Points

- Distinct from Type on Path: Path → Path deformation of the existing geometry into a circular
  strip. Angle −180..180° (default 0); Horizontal / Vertical selected axis (default Horizontal).
- The origin is only the midpoint of shared control bounds, including anchors and absolute Bézier
  handle endpoints across every contour. For selected-axis span L, local axial coordinate q and
  cross-axis coordinate w, φ = θq/L; the control-point mapping uses
  u′ = q sinc(φ) − w sin(φ), v′ = q cosc(φ) + w cos(φ), then adds the shared origin. Small φ uses
  series approximations for sinc and cosc. Relative handles are reconstructed from mapped absolute
  controls; this is not an exact warp of nonlinear cubic interiors. Resample Path first for finer
  geometry.
- Empty input, zero axial span and zero angle independently clone geometry. Groups, contour order,
  closure and metadata remain; generated IDs are deterministic. Non-finite vector/bounds/arithmetic,
  over-budget input, or a fold (1 − θw/L ≤ 0 at either cross-axis bounds endpoint) is rejected.
  Folds are not silently clamped. The modifier participates in Path bypass, unstable provenance,
  upstream Text editing restriction, Session history/locks and transactional geometry preflight.
- Focused evidence: 10 helper tests + 4 graph/Session tests + 1 DOM test + 1 disconnected
  validation test = 16/16 pass; Web build passes with the existing large-chunk warning. No Blender
  numeric parity, native GUI or full-suite claim.

Bend checkpoint playground: 41 serialized/reopened and evaluated layers now cover all16 modifiers.
The last row compares unchanged Text, Taper50% and Bend Points120° on the same NECT word.
The actual exported SVG was rasterized and visually inspected; this remains source-render evidence, not native GUI acceptance.

### Set Closure

- Path → Path with a Boolean **Closed** input (default true). It sets the closed flag on every
  contour, retaining contour order, groups, anchor positions and relative handles. Closing uses
  the last point's outgoing handle and the first point's incoming handle for the real final cubic
  segment; opening removes that segment while retaining both dormant vectors. Empty and one-point
  contours remain valid and finite. This does not merge endpoints or weld contours.
- Fill may draw an implicit final edge on an open path, but it leaves the path geometry open.
  Set Closure changes that geometry, so downstream Stroke and Boolean can use the real closing
  segment. Its graph and Inspector note makes this distinction explicit.
- Closure geometry preflight checks finite coordinates, finite handle endpoints and
  the 10,000-contour / 100,000-point limits before evaluation. Derived contour and point IDs are
  deterministic; the result is fingerprinted and marked unstable for downstream Remap. Standard
  Path bypass, transactional Session history/locks, and JSON/SVG behavior apply. Canvas text
  character editing remains upstream at Text.
- No sampling is performed. Focused geometry, core/Session and DOM evidence is recorded with this
  change; this does not establish native GUI or any other application's behavior.
- The current modifier catalog has 18 entries; the existing 41-layer playground remains unchanged
  and covers the previous 16. Focused Set Closure geometry/core/Session/DOM tests pass 9/9, the
  existing Join/Boolean regression passes 5/5, and the Web build succeeds with the existing large-
  chunk warning. This is source-level evidence, not native GUI acceptance.

Root integration keeps the existing Boolean evaluator unchanged; Set Closure validates its own geometry. Updated the existing bypass contract test list for the implemented Path→Path modifiers instead of retaining stale unsupported-node expectations.

Final Set Closure integration evidence: closure helper/core/DOM9 + refreshed existing bypass-contract40 =49/49 pass; final Web build passes. Existing Boolean evaluation semantics remain unchanged. The prior stale unsupported-Smooth-Path assertion is repaired by explicitly listing all implemented identity-bypass nodes; no production bypass guard was weakened.

### Twist Points

- Inspired by the effect category in [Adobe Illustrator's reshaping effects](https://helpx.adobe.com/illustrator/using/reshaping-objects-effects.html), which describes a stronger twist near its center. Nect's numeric mapping is its own smoothstep contract; Adobe numeric parity is not claimed.
- Path → Path with one angle control, −360..360° (default0). One shared bounds box uses every anchor and absolute Bézier control endpoint across all contours. The automatic center is the bounds midpoint and `R = 0.5 × hypot(width, height)`; there are no manual center or radius controls.
- For each absolute anchor/control point `p`, `u = clamp(distance(p, center) / R, 0, 1)`, `s = 3u² − 2u³`, and the original offset is rotated by `angle × (1 − s)`. Positive angle rotates clockwise in screen coordinates (Y down). Radial distance from the center is retained. Relative handles are reconstructed from their independently mapped absolute endpoints.
- Empty input, zero radius and zero angle independently return a cloned geometry. Contour order, groups, closure and metadata remain. Derived contour / point IDs are deterministic; output provenance is unstable for downstream Remap. Input is capped at 10,000 contours and 100,000 points; non-finite vectors, absolute endpoints, intermediates or output reject the whole edit. Bounded Session preflight preserves authored state on failure. Standard Path bypass, history/locks, JSON/SVG and upstream Text editing restrictions apply.
- This maps anchors and absolute Bézier controls separately, not the exact nonlinear interior of each cubic. Resample Path first to increase point density. A sparse rectangular outline whose corners all lie at R may show little or no visible change because points at R receive zero angle. No samples were added to the shared modifier playground.
- Focused helper8 + graph/Session/text4 + DOM1 + disconnected validation1 + bypass41 =55/55 pass; the Web build passes with the existing >500 kB chunk warning. Native Windows GUI acceptance and exact Adobe numeric matching are not claimed.

Twist checkpoint playground: all18 modifiers now have examples in47 serialized/reopened and evaluated layers. The final row compares a genuinely open stroke, Set Closure and a resampled twisted star. SVG path data distinguishes the first two by the closing z command; Inkscape PNG rendering confirms the open gap and closed edge. PyMuPDF incorrectly rendered the open polyline as closed in this case, so use Inkscape for this proof. Cloud Chromium launch was unavailable; no browser/native GUI acceptance is claimed.

### Direct curved Expand Stroke checkpoint

Expand Stroke now accepts bounded cubic centerlines directly using the existing adaptive subdivision core, without invoking Closed Path Offset region semantics. All-zero-handle inputs retain their prior fast path. Endpoint support vertices align open-cap tangent chords; curve/arc/grid components have explicit tolerances but no global final-boundary error guarantee. Focused Expand Stroke and Closed Offset checks pass49/49; formatting and Web build pass. The playground's Expand Stroke card now uses a cubic S-like path directly, and its actual filled outline SVG was inspected in Inkscape. The catalog remains18 modifiers and the example remains47 layers; this is an improvement of an existing node.

Grouped Offset integration proof: `scripts/grouped-offset-playground.js` creates four validated/reopened editable layers in `examples/grouped-offset.json`. Two overlapping ellipse instances stay independent before/after +10px miter offset. Actual SVG paths retain separate groups/0.45 alpha; Inkscape white-background rendering shows the darker overlap (transparent PNG pixels carry alpha115 vs178). A round-join version exceeded the existing aggregate estimated-join budget and was correctly rejected; limits were not raised.

Resample spacing example: two serialized/reopened editable layers are verified to produce6/6 samples in Count6 mode and6/11 samples at maximum spacing24px for120/240px open contours. Each includes a downstream Path Remap for inspecting anchors. The example explicitly uses schemaVersion12 and preserves dormant Count/Spacing controls.

### Auto Handles

- Distinct from source Curve smooth/symmetric coupling and Smooth Path anchor averaging: Path→Path Auto Handles preserves all anchor positions/count/order and replaces only derived handles.
- Strength0..1 (default1). Interior/closed tangent is next−previous; paired handle length is Strength × min(adjacent chord lengths)/3. Open endpoints use a one-sided edge vector/3 and an unused zero handle. Closed contours wrap neighbors; duplicate chords/zero tangents produce zero handles. Empty/singleton inputs are finite. Strength0 clears handles; same-type bypass restores original geometry.
- O(points),10000-contour/100000-point limit, immutable clones, deterministic IDs, finite coordinate/absolute-endpoint guards, unstable fingerprint and upstream Text edit restriction. Acute turns may overshoot or self-intersect; this is not topology repair or exact Blender parity. The [Blender automatic-handle category](https://docs.blender.org/manual/en/latest/modeling/curves/structure.html) is a reference, not the numeric formula.
- Final focused evidence: helper9 + core/Session2 + DOM1 + disconnected validation1 + bypass42 =55/55 pass; Webbuild passes with existing large-chunk warning. No native GUI/full-suite claim.
- User's curve-to-angular question was answered with actual core output in an8-layer editable comparison: original circle and24/8/4-point Resample variants. Inkscape rendered the SVG on white for the delivered PNG. Catalog19, main playground still18/47layers; this comparison is not a new production asset count.

### Instance on Points

- Inspired by Blender's [Instance on Points](https://docs.blender.org/manual/en/4.5/modeling/geometry_nodes/instances/instance_on_points.html)
  concept. Nect materializes Path geometry; it does not claim Blender reference-instance parity.
- Path → Path with required Source Path and Points Path inputs. The whole-source geometric
  bounds center, from the existing `pathBounds(source)`, is placed at each authored target
  anchor. The Boolean Align Tangent option defaults off and retains the original translation-only
  geometry. When on, source local +X follows target handle B when nonzero; otherwise the reverse
  of handle A; otherwise a centered neighbor chord, wrapped for closed contours and one-sided at
  open endpoints. If that chord is degenerate, the next edge, prior edge in path-forward direction,
  then +X are the fallbacks. Relative source handles rotate with the source. This does not rotate
  or scale the target anchors, and target groups / curve interiors are ignored. Curve interiors
  are not arc-length-sampled; use Resample Path upstream for even spacing or denser tangent anchors.
- Existing schema-version-1-to-12 Instance on Points documents backfill `alignTangent: false`
  and advance to schema version 13. This preserves their prior translation-only output; invalid
  non-Boolean values reject validation rather than silently coercing them.
- Copies preserve source contours, holes, groups, relative handles and metadata. Each target
  anchor, including a duplicate, gets distinct deterministic contour/point IDs and independent
  groups. Transparent copies remain separately painted; no global union is performed.
- Both inputs are required and incomplete wiring stays a normal editable state. Source-path
  bypass forwards the exact source without evaluating Points. The node participates in bounded
  transactional preflight; counts are rejected before Paper bounds work. Output budgets and
  non-finite arithmetic reject the full command. Derived output is unstable for downstream
  Path Remap, and direct Text character editing stays upstream at the Source Text.
- The [11-layer tangent playground](../examples/instance-tangent-playground.json) compares
  translation-only and tangent-aligned placements and reopens with identical evaluated output.
  Its SVG / Inkscape rendering was inspected. This improves the existing 20th modifier; it does
  not add a catalog entry, alter the 47-layer main playground, or change the separate frozen
  19-modifier trial.
- Focused helper / core / DOM tests and one Web build cover this change; full-suite, native
  Windows acceptance and replacement packaging are outside this slice. Older Instance on Points
  documents backfill false and save as schema 13.

### Halftone and Ordered Dither — luminance-to-geometry v1

- These are PaintedPath→PaintedPath modifiers after a solid Fill or the existing two-stop
  Linear / Radial Gradient Fill. Each has Cell Pitch 1–1024 px (default16), Invert Luminance
  (default off), and Grid Origin (Vector2 default `[0,0]`, each component ±100000 px). The paint source must have a solid
  Color or supported gradient and an ungrouped collection of closed contours. Open contours,
  explicit groups, stroke-only paint and Image inputs fail explicitly. Source fill/stroke styling
  is replaced by opaque black output geometry; the upstream source stays unchanged.
- Luminance is sampled at centers of every pitch-sized world-grid cell with positive-area
  intersection against the flattened source bounds. Centers can fall outside the mask; generated
  geometry is clipped afterward. Existing gradient paint interpolates encoded sRGB stop values;
  channels are then linearized, alpha is composited over white in linear light, and WCAG relative
  luminance is calculated with coefficients 0.2126 / 0.7152 / 0.0722. Darkness is `1 − L`; Invert
  switches to `L`.
- Halftone uses a centered disk clipped to each square cell, with polygon area calibrated to that
  cell's darkness. Its polygon segment count keeps circle sagitta at or below0.1 document unit.
  Ordered Dither compares darkness to the fixed 8×8 Bayer threshold `(rank + 0.5) / 64` and
  emits a full-cell square above threshold. This is ordered thresholding, not error diffusion.
  Both modes clip marks to the closed compound source mask, preserving holes and preventing
  marks from crossing the outline. Generated geometry is opaque black. Vector calibration does
  not claim printed dot gain, screen frequency or physical print calibration.
- Bounded work: at most4096 sampled cells; the mask helper allows32 contours /256 authored points,
  512 flattened vertices and4096 subdivision nodes; generated geometry is capped at100000
  vertices, mask-mark intersection at2,000,000 vertex-pair units, and clipped output at100000
  vertices /10000 contours. Exceeded budgets reject a command atomically. Cubic source curves are
  flattened at approximately0.1 px, so both the mask and circular dots are polygonal
  approximations. Same-type bypass, stable generated IDs, unstable downstream-Remap provenance,
  common Undo/Redo, JSON serialization and transaction preflight are included.
- Focused evidence covers white/black output, a small source wholly inside a cell, nonaligned
  black bounds, varying gradient dot area, Bayer density, compound holes/no-leak, unsupported
  inputs, budget rollback, deterministic identity, serialization/history, editor controls and
  bypass. These checks do not establish native Windows interaction or physical print behavior.
  This v1 slice intentionally added no generic Field port or extra mapping framework; the separate
  Line Screen mapping below reuses the same field and mask helpers.

### Line Screen — luminance-to-geometry v2

- Line Screen adds another PaintedPath→PaintedPath map on the same supported filled vector source.
  It reuses Cell Pitch 1–1024 px (default16), Invert Luminance (default off), Grid Origin
  (default `[0,0]`, each component ±100000 px), center sampling and opaque black output. The source must remain ungrouped and
  closed; open contours, explicit groups, stroke-only paint and images are rejected. The output
  intersects the source's existing compound mask, so holes remain empty.
- Horizontal emits one centered rectangle per nonwhite cell: full pitch across X and
  `darkness × pitch` along Y. Vertical swaps the dimensions. Its area is therefore
  `darkness × pitch²`; white samples are empty and black samples cover the full cell. Neighboring
  marks join into stepped variable-width stripes on the fixed grid. It does not continuously
  engrave a vector contour, and it is not a print-calibrated screen.
- This stripe motif takes conceptual inspiration from the PostScript Language Reference §7.4.4:
  Adobe describes a line-screen spot function based on distance from a line through a halftone
  cell's center, with white pixels growing away from that line. Nect's implementation instead
  directly sizes cell-sampled vector rectangles; it does not implement a PostScript/RIP spot
  function, device frequency, screen angle or print behavior.
  [Adobe PLRM, §7.4.4](https://www.adobe.com/jp/print/postscript/pdfs/PLRM.pdf#page=500)
- Line Screen uses the existing 4096-cell sampling cap, 100,000 generated/output-vertex caps,
  2,000,000 vertex-pair mask-intersection budget, deterministic output identity, unstable
  downstream-Remap provenance, typed bypass, schema-15 serialization, shared Undo/Redo and
  transactional preflight. Invalid orientation or over-budget upstream edits reject atomically.
- The [12-layer Line Screen playground](../examples/line-screen-playground.json) compares both
  directions against a shared gradient source and reopens with stable evaluated output. Its
  matching `scripts/line-screen-playground.js` builds and validates the editable JSON and SVG.
  Focused helper/core/DOM tests and one Web build are the bounded evidence for this addition;
  they do not establish native Windows input behavior or physical print output.

### Shared luminance grid origin — schema 15

- Halftone, Ordered Dither, and Line Screen expose a shared Vector2 Grid Origin. Its default
  `[0,0]` keeps the existing texture phase; each finite component is limited to ±100000 px. In
  Nect's document coordinates Y increases downward, so this is the top-left corner of grid cell
  `(0,0)`. It sets the cell lattice and sample centers; it does not translate source geometry or
  change source-mask coordinates. Shift it to phase-align multiple effects or move texture marks
  relative to artwork.
- The existing luminance sampler accepts an `{x,y}` origin. The core maps `[x,y]` to it and keeps
  Halftone dots, Bayer 8×8 rank selection by integer grid indices, horizontal/vertical Line Screen
  rectangles, compound-mask clipping and all existing limits unchanged. No rotation, image input,
  or new mark mode was added. The schema-14 `[0,0]` default fingerprint is preserved; non-default
  origins affect derived-output identity.
- Schema 14 and earlier luminance nodes backfill `[0,0]` and upgrade to schema 15 on validation;
  default output remains identical. New values serialize with the shared document and honor locks,
  Undo/Redo, bypass and bounded edit preflight. Direct and driven origins outside the finite
  bounds are rejected. The [12-layer grid-origin playground](../examples/luminance-grid-origin.json)
  compares 0, quarter-cell and half-cell shifts on the same unchanged source paths and has a
  serialized/reopened evaluation plus actual SVG/PNG review.
