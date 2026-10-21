# Repeater: bounded AE-style 2D mode

`Geometry Repeater` retains its default **linear**, **radial-fit** and
**radial-step** modes and their prior appearance. Select **ae-2d** to expose
Copies (`count`), Offset, Anchor, Position, Scale (XY %), Rotation (degrees),
Start/End Opacity (%) and Copies Above (later copies in front; unchecked behind).
Alternate Scale (%) defaults to100 and scales only odd zero-based copies1,3,5…
in `ae-2d`, independent of Offset and paint order. Its finite range is0.01–1000%.
It scales the source around Anchor before the existing copy transform, preserving
the transformed Anchor placement. Positions and Bezier handles scale together;
the step, radius/placement, opacity and stable derived IDs are retained. Linear
and radial affine configurations use the same rule. Nondefault/bound Alternate
Scale requires schema45; old documents backfill100 without advancing their version.
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

For an integer transform index, Negative Offset uses the inverse step. Both
Scale axes must be nonzero for a negative Offset. Negative Scale is a reflection
and alternates through integer powers. Zero Scale is allowed for nonnegative
Offset and collapses geometry.

Copies may be fractional. Nect materializes `ceil(Copies)` copies; the final
copy is multiplied by `Copies - floor(Copies)` alpha. Start/End Opacity still
interpolates across the rendered copy indices, so a fractional tail reaches
the endpoint opacity before its tail alpha is applied. `Copies = 0` is empty.

Offset may be fractional. Nect applies the integer power followed by one
bounded partial step. Position and Rotation are linearly interpolated and each
nonnegative Scale axis is geometrically interpolated. A negative fractional
Offset uses the inverse partial step. Fractional Offset with a negative Scale
is rejected because a negative scale has no single real fractional power;
negative Scale remains supported for integer transform indices. Zero Scale is
allowed for nonnegative Offset, but any negative Offset still requires both
Scale axes to be nonzero. These are explicit Nect rules, not a claim of AE
fractional-power compatibility.

Each copy gets fresh stable derived IDs; Copies Above reverses the order of
whole copies, preserving the order and winding of contours inside them. Holes
remain in their original independent group.

## Limits visible in the editor

- Copies and Offset are finite numbers from 0 to 256 and -256 to 256. The
  rendered-copy budget uses `ceil(Copies)`, including for fractional values.
  These values reject atomically, including when driven by expressions or wires.
- Scale -1000 to 1000% on each axis. Finite matrix/coordinate magnitude at most
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
singular/reflected scale edge cases. Nect's integer affine-power contract,
partial-step interpolation policy and safety limits above are explicit
implementation decisions. There is no AE runtime comparison here and no claim
of complete AE compatibility.

The operation-loop and chatgpt-web-double-check skills were invoked. External
consultation status: not-required (0 sends); official documentation, explicit
bounded behavior and local tests settle the implementation decisions.

## Verification (2026-10-18)

- `node --test tests/repeater-ae.test.js tests/creation.test.js tests/mirror.test.js tests/join.test.js`: 24 passing checks (9 dedicated Repeater checks).
- `npm run build`: passed. No native/EXE build was performed.
- `node tests/repeater-ae.e2e.js`: isolated headless Chrome, production bundle and
  native CSP. Inspector operation/fields, fractional Copies/Offset, negative Offset,
  Undo/Redo, real JSON save/reopen and SVG/PNG downloads passed. The test waits
  for deferred numeric-control rendering before the following input.
- Full 160×100 RGBA digests match for Canvas shapes, SVG and PNG:
  `a1245e675c33d1615b1d40b6a9308006e629639a58855e9435c2003d90c56138`.
  Copies Above and Below produce distinct digests. Browser errors: none. Machine evidence is in
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

### Integrated schema-37 verification

At the integration branch, 23 Repeater/creation/Mirror/Join checks pass,
including schema-36 Dither preservation, legacy Repeater backfill without a
version bump, promotion to 37, strict new-mode validation, Undo/Redo and JSON
reopen. The existing 97 recovery/text checks and 70 Dither/Points checks pass.
Production-CSP Edge verifies Repeater Inspector controls, schema promotion,
native downloads/reopen and the same full Canvas/SVG/PNG pixel digest above.
The Points, Dither, Graph clipboard, grapheme editing and per-operation recovery
production-browser regressions also pass. No second-batch EXE is built yet;
the parent-confirmed Snap commit remains the next integration dependency.
