# P02-D Packet B1 — Primitive authored-angle radial control

Status: CLOUD AUTOMATED QUALIFIED / REAL-GUI AND WINDOWS CANDIDATE PROOF PENDING. Windows baseline worker released all source/build ownership at its terminal receipt on2026-10-02 00:57 UTC.
Exact base: cb864804aaccfe73b205405db5185174c44e81e6.

## Authority and complete census

[Packet B](https://app.notion.com/p/3eafd279a6f38185bce6e73fcb46b0df)
and Confirmed Should [REQ-162](https://app.notion.com/p/3e0fd279a6f3810ab72acc0d73a6c2ac).
At the exact base, declared GUI degree properties are Polygon/Star
`generator.rotation`, selected point `in.angle`/`out.angle` (including generated
Point Edit targets), and Repeater operation rotation. Multi-selection surfaces
include point-angle batches, common primitive rotation across selected objects,
and matched Repeater rotation across selected object stacks.
Repeater alone already has a radial control. Transform Rotate-by and selection
transform angles are one-shot affine commands, not persistent authored Scalars;
point topology angle labels and Path Follow tangent frames are derived and excluded.

## Bounded implementation

Only Polygon/Star source rotation in this slice. Preserve exact Ref, existing
numeric editor and canonical EditProperties/Session gesture ownership. Add a narrow
primitive adapter and reuse RotationKnob without changing existing Repeater names,
callbacks or default indicator convention. Scope changes require explicit worker
non-overlap acknowledgement before source mutation. Point/handle/batch coverage is
an explicit follow-on, not silently counted complete.

Primitive authored angles are finite signed degrees, unwrapped within ±1e9;
invalid/out-of-range values reject rather than clamp. Geometry uses zero at local
+X and clockwise positive in Y-down coordinates. The primitive dial must explicitly
show that same zero reference; existing Repeater presentation remains unchanged.
Whole turns stay authored. Indicator modulo does not normalize the stored double.

Use one frozen Session/revision/source identity per interaction. Live binding or
expression sources disable/refuse the dial. One successful drag is one transaction;
Escape, no-net motion, invalid/stale context and teardown cancel safely. Avoid
canceling another control's unrelated active Session gesture. Preserve source and
point identities, native format, and independent numeric editing.

## Test-first acceptance

- Both Polygon and Star: exact shared numeric/dial Ref, 725 and negative/multi-turn
  values, indicator convention, numeric-to-dial and dial-to-numeric refresh.
- Continuous preview, one revision, Undo/redo, Escape/no-change/no-net cancellation.
- Binding/expression disabled controls, stale revision/document/source and concurrent
  gesture refusal, finite/range rejection without partial state.
- Native round-trip and source/point identities preserved; fixed hit geometry and
  meaningful accessible name. Existing Repeater behavior remains unchanged.
- Dedicated geometry-only test, relevant existing primitive/history/Window coverage,
  all-target build and full regression accounting; exact-source real-GUI evidence
  remains a separate Windows obligation.

## Continuity

Dot remains integration owner. Local Windows worker tasks roll over at useful
semantic boundaries, carrying only exact SHA/tree, toolchain, build/process state,
results/logs, remaining acceptance and explicit writer release. New local threads
must be placed in `Nect GraphicsTool` through a verified supported project route.
Project assignment is unverified; the later user relaxation below removes it as a blocking requirement. Do not substitute a title for binding.

## Narrow gesture-identity prerequisite

Review found an ABA ownership hole in the available API: active status alone cannot
distinguish a canceled dial preview from a replacement gesture at the same revision.
Add only a transient Session generation field/getter and successful-begin increment.
The new primitive adapter combines generation with frozen Host session identity.
Existing consumers, commands, native format and History remain unchanged. Generation
exhaustion rejects before mutation. Windows source/build scope was fully released
before this core change. Cancel/disarm synchronously at Inspector rebuild and Window
close/destruction while Host is alive, preserving unrelated numeric drafts.

User relaxed mandatory Nect GraphicsTool project placement on2026-10-02 00:58 UTC
after the supported project-route limitation was explained. Semantic-boundary fresh
local worker handoffs remain requested; project assignment is not a blocker.

## Qualified cloud receipt — 2026-10-02 01:30 UTC

- Polygon/Star controls share the numeric field's exact Ref and canonical gesture
  commands. Primitive +X/clockwise and unchanged Repeater zero-at-top rendering
  are checked from captured indicator pixels at normal/high DPI.
- Dedicated `primitive_angle_ui_interaction`113 checks and actual-DPR2
  `primitive_angle_ui_hidpi`114 checks PASS. Coverage includes signed/full-turn
  motion, live numeric preview, exact committed-state/identity preservation,
  Undo/redo, Escape, no-motion/no-net, binding/expression, stale source/document/
  revision, busy gestures and same-revision replacement-gesture protection.
- A primitive-only1e-10-degree delta tolerance removes floating accumulation residue
  from zero-winding returns. It does not normalize authored full turns or numeric
  entry. Range failure cancels/disarms the entire local interaction.
- Teardown invalidates before deferred widget destruction while Host is alive;
  close attempts cancel without invalidating the surviving row. The regression
  reuses the Inspector after close without rebuilding. Independent numeric drafts
  are not cleared by a dial that never began; this is not a claim that all generic
  drafts survive replacement of the Inspector.
- Long-fraction regressions exposed generic12-significant-digit display rounding.
  New primitive fields use17-digit round-trip text on rebuild and invalid-expression
  fallback, selected by a widget-only flag. Separate malformed-expression drafts
  remain retained, with exact native bytes/revision unchanged. Generic fields and
  existing Repeater formatting/callbacks are unchanged.
- Independent review findings were repaired and retested; no remaining blocker.
- Final all-target desktop build PASS; full76 CTest49pass/1skip/26fail. Failing names
  exactly match the Utility checkpoint, not a whole-suite pass. Fresh-process
  native/SVG smoke PASS; no formal MCP or real-GUI claim follows from smoke.

Ignored local evidence SHA-256:
- `primitive-angle-qualified-build.log`:994e2b9622008345ae07a0f25e9fa3311d6cf5f179c63018943a50f7ae109bf0
- `primitive-angle-qualified-tests.log`:0b80828b4db33b0366c3d561a71e31c44486d9de6b70e12cdc95b2632b1e3b77
- `primitive-angle-smoke.log`:7cd237678e4001117718f685f4c612cbbbcb217b79273c1ad43af6dfc8986cb4

Whole REQ-162 remains open. Besides single-point and batch surfaces, the existing
Repeater adapter still needs current-runtime validation of live numeric lookup and
safe interruption paths; its unchanged behavior is not upgraded by this B1 receipt.
