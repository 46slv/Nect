# P02-D Packet B — Selected-point handle-angle radial controls

Status: CLOUD AUTOMATED QUALIFIED / REAL-GUI AND WINDOWS REPLAY PENDING.

Base: `12b5863334c891efc302861f28b9285c87b4a174` / tree
`25b55029d6370de1a5c9ce13542bc9913d5c7a73`.

Authority: [Packet B](https://app.notion.com/p/3eafd279a6f38185bce6e73fcb46b0df)
and confirmed [REQ-162](https://app.notion.com/p/3e0fd279a6f3810ab72acc0d73a6c2ac).
This slice is only for one selected point's authored `in.angle` and `out.angle`.
Point-angle batches, common primitive rotation, and matched Repeater batches remain
later work. No core, schema, transport, or native format changed.

## Bounded implementation

- Adds a radial control beside each selected point's existing numeric `in.angle` /
  `out.angle` field. The dial and numeric editor retain the identical structured
  `Ref`; the existing `EditProperties` / Session gesture path remains the sole
  authoring route.
- The point-angle indicator uses local `+X` as zero and clockwise-positive degrees
  in the Canvas's Y-down coordinates. Its visual and accessible indicator wraps
  modulo 360; the authored number stays signed, unwrapped, and exact. Point numeric
  display uses 17-digit round-trip text.
- Initial values come from the Inspector's evaluated snapshot. Generated Circle,
  Ellipse, Polygon, and Star point targets do not get a display-only Scalar. A real
  edit uses the existing canonical Point Edit path; it preserves the source and
  stable point ID and writes only the requested angle field. No-motion and no-net
  interactions do not create/activate a Point Edit or leave History changes.
- Existing angle binding/expression sources disable/refuse the dial, including a
  bypassed Point Edit source whose visible evaluated fallback is numeric. Point
  Edit enabled-driver handling remains governed by the core's authored-literal
  guard. Zero-length handles remain editable; `CanvasEvaluatedPoint.driven` is not
  used as an angle-source policy.
- The adapter freezes Host Session/document/revision, object and source identity,
  point/contour identity, and the canonical Point Edit destination. Dirty numeric
  drafts refuse dial start. Range/stale/source failures cancel without partial
  state. A live dial does not steal another control's gesture; Inspector rebuild
  and Window close cancel the dial's own preview.
- No angle dial is added for a multi-point selection.

## Focused acceptance

The dedicated `point_angle_ui_tests` target covers authored Path points and generated
Circle/Ellipse/Polygon/Star points; incoming and outgoing exact Refs; signed/full-turn
numeric values and modulo indicator; live numeric/dial synchronization; one revision,
Undo/redo, Escape, no-motion/no-net; zero-length handles; exact canonical Point Edit
identity and field; binding/expression and bypassed-source guards; Point Edit enabled
driver semantics; dirty drafts and ±1e9 range failure; stale revision, document,
Host Session, contour, source, topology, and Point Edit destination; same-revision
gesture ABA; primitive/Repeater/other-point dial arbitration; multi-point absence; and
rebuild/close/reuse. The high-DPI run checks actual widget DPR >= 1.9.

Release verification:

- `point_angle_ui_interaction`: PASS, 85 checks.
- `point_angle_ui_hidpi`: PASS, 86 checks at `QT_SCALE_FACTOR=2`.
- Focused CTest: 2/2 PASS.
- All-target Release build: PASS.
- Full CTest: 80 tests, 53 passed, 1 skipped, 26 failed. The failed-name set is
  exactly the same 26 failures as the recorded 78-test baseline; no new failure was
  introduced. This is not a full-suite pass.
- `scripts/smoke.py --exe build/d0-core/nect`: PASS for the headless real-process
  native/SVG scope; it reports `mcp_tested=false` and `gui_tested=false`.
- No exact-candidate native GUI, Windows runtime, or subjective visual acceptance is
  claimed by these automated checks.

Evidence log SHA-256:

- `build/d0-evidence/point-angle-final-all-build.log` —
  `9a8d7346e54374cd85bb07f9378e7c3610df45f08a3fd4e2d1b4ffc1aa6058ef`
- `build/d0-evidence/point-angle-final-focused-ctest.log` —
  `fa24fb7bbbf37862307f8464951fce92cde9075b52eef91d0e443c392cf71ef1`
- `build/d0-evidence/point-angle-final-normal.log` —
  `f8b09449a75203eb4219057184331068e6868ba8f9e7e373092045edfc0371e9`
- `build/d0-evidence/point-angle-final-hidpi.log` —
  `6d14235dbceb08af0ff98fc376c687ed1c0592b163d9c6402a72d296b50df330`
- `build/d0-evidence/point-angle-final-full-tests.log` —
  `924004153871b9c43b892d2db99885b3a049102299bfdecd162119e335a3e50a`
- `build/d0-evidence/point-angle-final-smoke.log` —
  `7cd237678e4001117718f685f4c612cbbbcb217b79273c1ad43af6dfc8986cb4`

The enabled-driver negative fixture must use the model's canonical Point Edit ID.
Core validation requires `<source-id>-point-edit`; a focused negative assertion records
that a noncanonical fixture is rejected with `INVALID_POINT_EDIT`. The earlier
malformed-fixture crash had no captured stack, so no stack-proven cause is claimed.

A separate close/reuse fixture initially timed out with synthetic Host Session identities.
The isolated fresh-Host close fixture passed; no modal inspection or stack established
the earlier timeout cause. It is not represented as a production fix.

Whole REQ-162 remains open.
