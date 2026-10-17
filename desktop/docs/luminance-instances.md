# Luminance Instances — bounded size and angle mapping

Implemented in the advancing development source, schema 20. This does not change the frozen native b9fe3f6 or browser cec1104 trials. Native/browser interaction acceptance remains open.

## Contract

- Source fill: filled, closed, ungrouped vector Path with solid/two-stop linear or radial gradient paint, or an embedded supported noninterlaced 8-bit RGB/RGBA PNG. Stroke-only, open or grouped vector source masks reject. A valid empty painted source yields an empty Path.
- Motif: a Path, including open/cubic, compound and grouped geometry. Convert painted artwork with Convert to Path first. The entire motif's exact geometric bounds center is the pivot. No auto-fit to pitch: scale multiplies authored motif dimensions.
- Cell Pitch: 1–1024 document px, default 32. Grid Origin is cell `(0,0)`'s top-left corner in document coordinates, each component within ±100000 px. Shared vector/image samplers provide center darkness, with sRGB-to-linear light and alpha composited over white; image samples use nearest texels. PNG processing retains the shared 4-megapixel cap.
- Select centers inside or exactly on the source boundary. Vector selection uses nonzero winding on the existing ~0.1 px flattened mask, including oppositely wound holes; image selection uses the placed rectangle, not alpha contours. A partial edge cell is omitted if its center is outside. The complete accepted motif is emitted without clipping, so it can extend beyond the source/cell or across a hole. This deliberate rule preserves editable cubic geometry.
- Scale at white / black: finite 0–100 multipliers, default 0.2 / 1. Angle at white / black: finite ±360 degrees, default 0 / 90. For sampled darkness `d`, each value is `white + (black − white) × d`. Endpoints can descend; these names refer to darkness endpoints, not sorted numerical extrema. Invert flips darkness, swapping which endpoint white/black source paint reaches. Positive angle is clockwise in Y-down document coordinates. Rotation is direct linear interpolation, without shortest-arc wrapping.
- A computed zero scale omits that copy completely. White therefore produces a small motif with defaults, not empty space. Set Scale at white to zero to omit white.
- Output is an unpainted materialized Path, not a raster and not a lazy instance object. Apply Fill/Stroke afterward. Relative cubic handles scale and rotate with anchors; each motif group remains separate per copy. Deterministic node/instance/contour/point identities use the existing Instance on Points machinery. Upstream mapping/motif changes invalidate downstream Path Remap provenance rather than silently redirecting retained edits. Character editing stays on source Text.
- Bypass passes Motif unchanged and does not evaluate the source or mapping controls, including with an Image source. Both inputs are required when active.

## Bounded work and compatibility

The shared sampled grid cap remains 4096 cells. Mask center testing is capped at 2,000,000 edge/cell pairs. Existing Instance on Points output caps apply: 256 nonzero copies, 256 groups, 10,000 contours and 100,000 points; point-instance work stays below its existing 2,000,000 limit. Grouped motifs may reach the group cap before 256 copies. Source masks retain their existing contour/point/flattening budgets. Excessive output, malformed geometry and nonfinite arithmetic reject atomically, with transactional parameter edits rolled back. No truncated result or larger budget is introduced.

Only documents containing this new node promote to schema 20. No parameters/defaults/geometry/fingerprints change for existing nodes. The existing Instance on Points translation-only and tangent modes retain their original arithmetic; a separate optional per-instance transform input powers this new consumer. This is not a generic Field system, arbitrary mapping curve, random distribution, density control, nested instancing, or Blender parity claim.

## Evidence

- `tests/luminance-instances.test.js`: endpoint mapping, exact handles, zero omission, metadata/IDs/groups, finite/instance/group limits, holes and partial-cell policy, mask work, typed sockets, bypass, empty/open sources, migration, transactional budget rollback, Undo/Redo, Path Remap suspension and fresh-process reopen.
- `tests/luminance-instances-image.test.js`: real embedded PNG, transparent-white/black samples, angle and scale, partial image cells, invert, bypass, schema20 and exact reopened geometry.
- `tests/luminance-instances-dom.test.js`: add-menu discovery, typed inputs, endpoint bounds and explanatory controls.
- `scripts/luminance-instances-playground.js` builds [editable comparison JSON](../examples/luminance-instances.json), [actual SVG](../artifacts/luminance-instances.svg) and verifies all 12 layers after reopening plus 80 motif outputs in a fresh Node process. Fresh-process comparison is byte-exact serialized geometry (JSON normalizes signed zero). [PNG](../artifacts/luminance-instances.png) is rendered from that SVG with Inkscape and visually reviewed; it is not a separate mockup.

Focused regression tests and one Web build cover this slice; no full suite or native/browser QA is claimed.

## Primary-source vocabulary

Blender documents per-point placement with scale/rotation inputs in [Instance on Points](https://docs.blender.org/manual/id/4.0/modeling/geometry_nodes/instances/instance_on_points.html), transform scale and center in [Scale Instances](https://docs.blender.org/manual/sl/4.0/modeling/geometry_nodes/instances/scale_instances.html), and rotation/pivot in [Rotate Instances](https://docs.blender.org/manual/en/4.0/modeling/geometry_nodes/instances/rotate_instances.html). These inform vocabulary only: Nect emits concrete 2D Paths with one bounds-center pivot and bounded sampled darkness endpoints, rather than Blender's field-driven 3D instance domain.
