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

- Resample Path: count-controlled equally spaced samples along each contour, as a foundation
  for deliberate point-density editing and later displacement. Curves become polylines; name
  and docs must make approximation explicit. Reference:
  [Blender Resample Curve](https://docs.blender.org/manual/en/5.0/modeling/geometry_nodes/curve/operations/resample_curve.html).
- Simplify Path: error-tolerance-driven point reduction with explicit topology checks;
  [Blender Decimate](https://docs.blender.org/manual/en/4.4/modeling/modifiers/generate/decimate.html)
  is inspiration, not an implementation contract.
- Expand Stroke: geometry outlines from width/caps/joins, then Boolean-editable shapes;
  [Blender Solidify](https://docs.blender.org/manual/en/5.3/modeling/modifiers/generate/solidify.html)
  motivates converting thickness into geometry. Need a bounded open/closed offset contract.

These are an evolving implementation sequence under Shiro's explicit research/addition
instruction, not a reason to rebuild existing Repeater/Offset/Mirror or run full audits.

### Resample Path

- Implements the count-based concept from the cited Blender Resample Curve reference as a
  2D Path node: 2–1024 points per contour (closed contours require at least 3), default 32.
- Samples at equal intervals of the source's numerically evaluated arc length using the
  existing Paper.js geometry engine; output handles are zero. Open endpoints retained;
  closed paths avoid duplicating the seam. Zero-length contours remain finite.
- This creates a polyline approximation. Low counts can remove corners/small features or
  alter shape topology; it is not tolerance-preserving Simplify. Source remains editable.
- Preserves groups and closed state, namespaces deterministic generated IDs, and uses the
  existing unstable fingerprint/remap protection. Output limits 10000 contours /100000
  points plus a 2,000,000 source-points × samples work estimate across contours.
- The common bounded-modifier preflight now includes Mirror, Corner Round and Resample
  alongside Offset. Invalid newly introduced geometry errors roll back commands; missing
  wiring stays editable and pre-existing errors can be repaired.
- Evidence: Resample core5/5 + DOM1/1, Corner Round Session3/3 and Offset pipeline12/12
  passed. Native GUI and raster visual review remain open. Distributed EXE unchanged.

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
- Solid straight-edge input only; Resample Path before Stroke converts curved input explicitly.
  All-zero dash patterns count as solid; any nonzero pattern rejects. Open caps support butt,
  square and round. Closed contours use Clipper's
  [EndType.Joined](https://angusj.com/clipper2/Docs/Units/Clipper/Types/EndType.htm) for centered
  stroke bands with holes, not Polygon inflation. Earlier open-only research is superseded by
  concrete Joined rectangle/reversed-winding probes and tests.
- Independent source groups are processed independently; overlapping outlines within a group
  are unioned by the pinned Clipper kernel. Width 0 is empty; nonzero width below 0.0002 px
  rejects. Miter limit 1–100. Input32 contours/256points, output2048points, round-work4096.
- Local integer grid 0.0001px; round-arc tolerance at most0.1px. Conservative coordinate/width
  envelope prevents unsafe integer/world restoration. No exact SVG-stroke equivalence claim.
- Evidence: kernel9/9 + graph/Session3/3 + DOM1/1, Web build pass. Closed rectangle band area,
  hole winding, caps, Boolean input, groups, unsupported input and atomic dash rejection covered.
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

Next: Subdivide Path to add Bézier anchors without changing the curve shape, rather than converting
it to a polyline. See [Blender Subdivide Curve](https://docs.blender.org/manual/nl/4.5/modeling/geometry_nodes/curve/operations/subdivide_curve.html).
Use bounded per-edge cuts and retained cubic subsegments; preserve endpoints/closed seams and test
sampled shape equivalence before adding it to the catalog.
