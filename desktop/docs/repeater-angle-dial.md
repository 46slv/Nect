# Geometry Repeater rotation dial

Production Desktop's Inspector adds a compact radial control beside the Geometry
Repeater rotation number field. It is an adapter to the existing
`Geometry Repeater.params.rotation` Scalar, not another authored property. Graph
numeric controls address that same Scalar; the dial is Inspector-only.

## Editing contract

- Authored degrees remain exact signed, unwrapped JavaScript numbers. Numeric
  display and ordinary horizontal scrubbing for this rotation do not apply the
  generic three-decimal display or 15-significant-digit scrub rounding. The
  dial's indicator alone displays modulo 360, so 725° points at 5° and 735° at 15°.
- 0° points right. Positive rotation is clockwise in the editor's downward-Y
  screen/document convention. Each pointer sample contributes the shortest
  signed angular delta from the previous sample; repeated travel accumulates
  whole turns. The authored start comes from the model, never the number widget.
- A 4-CSS-pixel center dead zone suspends angular continuity. Leaving it establishes
  a new pointer angle without a jump or a change to the accumulated delta.
- A pending numeric draft in the same row first uses the existing numeric change
  command. A valid typed edit and the subsequent drag remain separate Undo steps.
  Blank/nonfinite drafts are rejected instead of becoming a zero baseline.
- Pointer previews use CommandSession's existing exclusive parameter gesture with
  `authoredBase: true`. Preview state is detached from the authored document;
  the captured dial is not structurally rerendered. One completed drag commits
  once. Returning to the start makes no history entry. Escape, pointer cancellation,
  lost capture, focus loss and detached/stale targets discard the preview without
  changing the authored revision or Undo/Redo history. A late release cannot commit
  a cancelled gesture. Final pointer release clears the interaction before releasing
  capture, so the resulting lost-capture event cannot cancel the committed edit.
- Arrow Right/Up adds 1° and Left/Down subtracts 1°. Shift uses 10°, Alt uses 0.1°.
  Each key event is one Session gesture/Undo; arrow events are consumed only on the
  dial, leaving numeric text entry and other shortcuts intact. Focus is restored
  to the dial after a key commit.
- A connection/expression does **not** disable Base editing in the current Desktop.
  The dial and adjacent Base input edit the same fallback value. The existing
  result readout and paused-expression label distinguish Base from the effective
  output. During a drag, that result readout refreshes from the same validated
  preview evaluation as Canvas, without replacing the captured control. Source IDs/ports, active/dormant expression text and connection priority
  remain unchanged. Existing composition/parameter locks still reject editing.
- Limits are the current Desktop's finite Scalar and evaluated-geometry rules.
  No legacy ±1e9 bound or schema migration is introduced. Model commands remain
  responsible for atomic validation and error rejection.

The stable 36×36-CSS-pixel hit target has a named slider role, full authored
value/readout, disabled state, keyboard focus outline and pointer hover/drag state.
Only the internal indicator rotates; hover does not resize the hit rectangle.

## Scope and historical adaptation

This implements the radial/numeric interaction from legacy P02-D contract 3 and
`D-KNOB-02` in `docs/p02-d-entry.md`, following the accepted design in
`docs/p02-design-gate.md`. The old `op.<id>.rotation` Ref and Qt driven-disable/
±1e9 semantics are not the production Desktop model. Existing Qt acceptance does
not establish Desktop/WebView2 acceptance. No other angle controls, final panel
redesign, dependencies, schema, evaluation or Session ownership changes are made.

## Evidence and remaining acceptance

Focused local checks:

- `node --test tests/repeater-angle-dial.test.js tests/gesture-session.test.js tests/canvas-gesture-session.test.js tests/repeater-ae.test.js`: 26/26.
- `node --test tests/repeater-angle-dial-dom.test.js`: 1/1 synthetic DOM integration
  case. It covers 725→735 with 15° indicator and one Undo/Redo, full/negative turns,
  both seams, center transit, exact tiny starting values, pending numeric drafts,
  numeric invalid input, same/different-row numeric focus settlement, live driven results,
  all cancellation routes, preserved history/revision,
  isolated keyboard focus, Base/result priority and locked/stale/deleted controls.

Final local verification also ran the existing numeric-scrub and authored-value
DOM regressions alongside those focused tests, serially: 29/29 passed. The full
`npm run format:check`, production `npm run build` and `git diff --check` passed.
Vite retained its existing large-chunk warning; no dependencies were changed.

Synthetic DOM and headless command checks are not rendered browser or native
WebView2 evidence. A separate rendered Chromium check on 2026-10-22 opened a
synthetic vector document through Project/Open and verified keyboard 725° → 735°,
real-pointer quarter and full turns, one-step Undo/Redo, and a pending numeric
draft retaining 725.0000000000001 across the separate typed-edit/drag Undo steps.
The full turn retained the unwrapped value while returning the indicator to the
same angle. Native Windows/WebView2 input, held-pointer Escape in a real browser,
and 100%/200% display qualification remain unverified by that rendered check.
