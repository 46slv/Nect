# Repeater: bounded AE-style 2D mode

`Geometry Repeater` retains its default **linear**, **radial-fit** and
**radial-step** modes and their prior appearance. Select **ae-2d** to expose
Copies (`count`), Offset, Anchor, Position, Scale (XY %), Rotation (degrees),
Start/End Opacity (%) and Copies Above (later copies in front; unchecked behind).
Existing documents receive identity defaults for the new controls when loaded.
Save editable JSON to retain the procedural graph. The new mode uses schema 37;
older applications reject that project version.

## Transform contract

Copies includes the original: 0 produces an empty path; 1 uses Start Opacity.
For copy index `i` starting at 0, use `n = i + Offset`. Define one affine step
as `M = T(Anchor + Position) R(Rotation) S(Scale / 100) T(-Anchor)`.
Nect applies `M^n` to positions and its linear part to relative Bezier handles.
Thus a translated, rotated or nonuniformly scaled step changes the following
step's displacement; transforms accumulate, rather than adding independent
per-copy TRS values. Offset moves the transformation index, without moving
opacity's first/last endpoints. The source remains immutable.

Negative Offset uses the inverse step. Both Scale axes must be nonzero for a
negative Offset. Negative Scale is a reflection and alternates through integer
powers. Zero Scale is allowed for nonnegative Offset and collapses geometry.
Start/End Opacity interpolates linearly by copy index. Each copy gets fresh
stable derived IDs; Copies Above reverses the order of whole copies, preserving
the order and winding of contours inside them. Holes remain in their original
independent group.

## Limits visible in the editor

- Only integer Copies 0–256 and integer Offset −256–256. Fractional copies and
  offsets reject atomically, including when driven by expressions or wires.
- Scale −1000–1000% on each axis. Finite matrix/coordinate magnitude at most
  1e12; excessive inverse/accumulated transforms reject instead of overflowing.
- Shared output caps: 256 independent groups, 10000 contours, 100000 points.
- Geometry comes before paint. After this mode, only Transform, Fill, Stroke,
  Linear/Radial Gradient Fill and Output are supported. Other geometry operators
  reject explicitly, so they cannot silently discard copy opacity or order.
  Bypassed operators preserve their input. Nested AE-style 2D repeaters reject;
  use legacy repeaters before the new mode to form a grid.
- Opacity applies to each independent contour group as a unit, including its
  Fill and Stroke. It multiplies their existing alpha. It does not isolate all
  source groups into a single composite. Gradients remain in artboard space.
- No repeated painted input, AE paint-operation stack, 3D, animation, timeline,
  expressions from AE, or native AE project interchange. Text remains editable
  at its source; direct character manipulation through Repeater is unavailable.

## Adobe reference and compatibility boundary

Checked 2026-10-18 against Adobe's current official
[Using the Repeater to replicate shapes](https://helpx.adobe.com/after-effects/desktop/drawing-painting-and-paths/shapes-and-shape-attributes/shape-attributes-paint-operations-path.html#using_the_repeater_to_replicate_shapes).
Adobe describes virtual copies, zero-based cumulative transformations, Offset
as a shift in copy transformation index, front/behind Composite order, and
interpolated endpoint opacity. These establish the feature direction.
Adobe's help does not specify fractional copies, fractional affine powers or
singular/reflected scale edge cases. Nect's integer affine-power contract and
safety limits above are explicit implementation decisions. There is no AE
runtime comparison here and no claim of complete AE compatibility.

The operation-loop and chatgpt-web-double-check skills were invoked. External
consultation status: not-required (0 sends); official documentation, explicit
bounded behavior and local tests settle the implementation decisions.

## Verification (2026-10-18)

- `node --test tests/repeater-ae.test.js tests/creation.test.js tests/mirror.test.js tests/join.test.js`: 22 passing checks (7 dedicated Repeater checks).
- `npm run build`: passed. No native/EXE build was performed.
- `node tests/repeater-ae.e2e.js`: isolated headless Chrome, production bundle and
  native CSP. Inspector operation/fields, integer refusal, negative Offset,
  Undo/Redo, real JSON save/reopen and SVG/PNG downloads passed. The test waits
  for deferred numeric-control rendering before the following input.
- Full 160×100 RGBA digests match for Canvas shapes, SVG and PNG:
  `d0ab49573707fe4b7005e49a21f8766b8d8f2ffd278f68925b2846139854dd4b`.
  A fill/stroke overlap changes from `[62,0,193,135]` (Above) to
  `[85,0,170,135]` (Below). Browser errors: none. Machine evidence is in
  `repeater-ae-verification.json`; local preview/downloads are under
  `test-results/repeater-ae/`.
- The additional `linear-gradient-pipeline.test.js` run has one pre-existing
  failure at line 89: it expects Join Geometry to reject painted input, although
  schema 31 supports that route. The same single test fails against the original
  base core. It was not changed in this task.

Integration: `core.js` and `main.js` are provided in a separate integration
commit/patch. Integration assigns schema 37 to Repeater while retaining schema 36
for Dither/Points. Legacy schema-36 Repeater nodes receive new defaults without
advancing their saved version; selecting `ae-2d` promotes to 37. No Dither,
Instance on Points, autosave, native binding or private font files were edited.
