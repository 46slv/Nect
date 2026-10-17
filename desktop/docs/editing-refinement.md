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
