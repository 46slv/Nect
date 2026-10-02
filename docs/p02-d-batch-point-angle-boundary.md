# P02-D Packet B — Multi-point handle-angle radial controls

Status: CLOUD AUTOMATED QUALIFIED. Exact-candidate real-GUI and Windows replay remain pending.

Base: `774a12ce2d2f94c5faef77a590c1f1fe50bdf633` / tree
`2bee9c91ee8de6ad2d359e5f6fe6f28c010e1672`.

Authority: [Packet B](https://app.notion.com/p/3eafd279a6f38185bce6e73fcb46b0df),
[REQ-162](https://app.notion.com/p/3e0fd279a6f3810ab72acc0d73a6c2ac), and
[P03 §6/U02](https://app.notion.com/p/3e3fd279a6f381ee8ab0ea5ea614c006).
This slice adds only multi-point `in.angle` and `out.angle` controls. Common primitive
rotation batches and matched Repeater batches remain outside scope. No core, schema,
transport or native format changed.

## Bounded implementation

- Adds a radial dial after each multi-point numeric angle row. The control and numeric
  row carry the same complete ordered target vector; the dial does not act on only the
  first point. One selection can span authored contours and different generated Path
  sources.
- The dial applies one common relative angular delta to every captured target using one
  `EditProperties{targets, delta, true}` per preview. Session recomputes each preview
  from the committed gesture snapshot, preserving each exact starting difference rather
  than accumulating earlier preview values.
- Uniform display requires exact equality of all unwrapped starting values. A common
  angle shows its modulo direction and current exact signed value. Different values stay
  `Mixed`, including values such as 5° and 365° with the same modulo direction. Numeric
  absolute entry, `+=` / `-=`, expression and source actions retain their existing
  behavior; point-angle numeric rows use 17-digit round-trip text.
- Mixed controls identify `Mixed`, the selected target count and relative behavior both
  visually and accessibly. The visible caption reports signed Δ during a drag. For a
  common angle, the accessible unwrapped value and Δ update with live previews and
  return to the committed value on cancellation.
- The adapter freezes the complete Ref vector, Host Session/document/revision, selection,
  active frame, each object and point, contour identity, source ID/type/version, and
  canonical Point Edit presence/ID/version. Each validation uses one evaluated snapshot
  for all refs and generated topology. Any stale target, driven angle or range failure
  cancels the complete gesture. Stored binding/expression checks include bypassed Point
  Edits. Typed Point Edit enabled-driver eligibility remains governed by the core's
  authored-literal policy.
- Generation ownership and cancellation collection prevent an old callback from
  updating or cancelling a same-revision replacement gesture. Disposed deferred widgets
  cannot start new gestures. Dirty numeric drafts are preserved. No-motion and
  return-to-start paths update the Session with an empty command list; they never send a
  zero relative edit that could create a ghost Point Edit. A genuine 360° drag still
  commits.

## Focused acceptance

The new `batch_point_angle_ui_tests` target reports 95 checks and covers:

- Exact full target-vector lookup and shared numeric/dial Refs; mixed `[-355°, 5°, 725°]`
  plus Δ10° becomes `[-345°, 15°, 735°]`. Repeated 8° → 12° → 10° previews are
  recomputed from the initial snapshot.
- Absolute numeric 45°, relative `+=10` on both common and mixed values, a nontrivial
  17-digit common value, one Undo/Redo, and native save/reopen.
- Exact values 5° and 365° remain visibly and accessibly `Mixed` even though their
  modulo directions match.
- Incoming/outgoing independence, unselected point preservation, authored points across
  contours, and generated Circle/Ellipse/Polygon/Star points. A single batch also spans
  an authored Path, Circle and Ellipse with separate canonical Point Edit destinations.
- Click, Escape and no-net gestures, including assertions that a real provisional
  generated Point Edit exists during preview and disappears afterward; preservation of
  an existing bypassed literal correction; and a genuine 360° commit.
- Any driven/bypassed-driven angle disables the full batch. A later typed-disabled target
  atomically rejects earlier authored and generated targets; later authored and generated
  out-of-range targets reject without leaking earlier previews. Typed enabled-driver
  allowed behavior is covered too.
- Stale revision, Host Session/document, authored contour, source ID/type, generated
  topology and Point Edit destination; same-revision gesture ABA, two-dial arbitration,
  dirty drafts, Inspector rebuild, Window close/reuse, and actual device-pixel ratio >=1.9.

The existing `point_angle_ui_tests` multi-selection assertion now distinguishes the
absence of single-point dials from the two visible batch dials. It remains at 85 checks.

## Verification

- Focused CTest: 4/4 pass (`point_angle_ui_interaction`, `point_angle_ui_hidpi`,
  `batch_point_angle_ui_interaction`, `batch_point_angle_ui_hidpi`).
- All-target desktop Release build: PASS.
- Full CTest: 82 tests, 55 passed, 1 skipped, 26 failed. The complete failed-name set
  exactly matches the saved point-angle baseline; this is not a whole-suite pass.
- `scripts/smoke.py --exe build/d0-core/nect`: PASS for the headless fresh-process native
  validation and SVG anchor scope. It explicitly reports `mcp_tested=false` and
  `gui_tested=false`.
- Independent read-only review of the exact source/test hashes found no remaining
  concrete blocker. No exact-candidate real-GUI or Windows result is claimed by this
  receipt.

Final evidence log SHA-256:

- `build/d0-evidence/batch-angle-final-all-build.log` —
  `091dfbeb38b7ccd95cdb9ba97d51c58291e99816295aded1898c4b9faa0ac1ee`
- `build/d0-evidence/batch-angle-final-focused-ctest.log` —
  `836126d80751b014e583fc3712ce7b2b86e8c538ecfd38a5501b32f08e1c9104`
- `build/d0-evidence/batch-angle-final-full-tests.log` —
  `990866c3ba01b3eebc6061ba8f3835199055aef58ed6622db6ad5ac3228f3ede`
- `build/d0-evidence/batch-angle-final-smoke.log` —
  `7cd237678e4001117718f685f4c612cbbbcb217b79273c1ad43af6dfc8986cb4`

Whole REQ-162 remains open.
