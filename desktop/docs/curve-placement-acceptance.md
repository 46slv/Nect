# Curve placement acceptance diagnosis

## Observed boundary

The Windows browser chain in [run 39831795057](https://github.com/46slv/Nect/actions/runs/39831795057),
head `19554828970256c77a8e0c676efe04614ecedce0`, stops in the open-Curve placement
assertion after actual-size, zoom-out, middle-button pan, then Alt-drag.
The requested artboard delta is `[18.75, -11.25]`; every authored anchor instead
moves by `[18.75, 5.79375]`. The failure retains handles and the rigid batch shape,
but the wrong translation is still a product acceptance failure.

The saved `workflow-curve-placement-failure.json` records the intended and
delivered inputs, full before/after documents and capture/bubble event traces.
All those event-boundary CTMs are `[5/6, 0, 0, 5/6, 337, 100.171875]`, and the
delivered world coordinates move from `[420, 210]` to `[438.75, 198.75]`.
This is not evidence that every internal CTM read was identical.

## Snapping is not a valid expected-output adjustment

The failure's document has no `editor` override. `guideDefaults()` therefore
disables Snap. Every traced down/move/up event also has `altKey: true`.
`snapTranslation` returns the input delta unchanged in either case; automatic
direction snapping is disabled by the same Alt state. The red reference card's
bottom edge being at Y=220 does not establish that the Curve snapped to it.
Do not adjust the expected movement, disable an assertion or replace the
all-anchor/exact-document checks with a weaker shape-only test.

## Ordering defect and bounded repair

Before the repair, whole-artwork pointerdown selected the artwork and called
`render` before reading `drag.start = canvasPoint(event)`. That render replaces
the artboard SVG and rebuilds layout, including dynamic graph navigation
controls. The handler now captures the event's current artboard-space position
before focus, selection or render, and reuses it as the gesture origin. Empty
canvas selection and every existing movement/selection command stay unchanged.

One hypothesis is an internal start-point CTM different from the final visible
CTM. The observed final translation would imply start Y=192.95625, rather than 210.
At the traced scale that corresponds to a CTM translation Y=114.375,
14.203125 CSS px below the event-boundary value. This is an inferred numeric
signature, not an observed CTM or a proven explanation of the CI failure.

`tests/helpers/curve-placement-trace.js` now records each native artboard
`getScreenCTM` result with its call stack and last observed event boundary, in
addition to the existing input trace. It returns the same native matrix object,
adds no geometry reads and does not change application state or rendered DOM.
The bounded `ctmReads` array is included in both successful placement receipts
and failure evidence. Its unit test demonstrates how a different internal read
can coexist with equal capture/bubble samples, while checking matrix identity,
argument forwarding, bounds and restart behavior.

## Deterministic regression and remaining acceptance

The `Curve pointer origin survives selection replacing SVG with transient
geometry` DOM scenario supplies stable CTMs for the incoming event target and
later moves, but a different CTM for the actual SVG replacement during the
selection/render handler. Happy DOM does not perform browser layout, so this
explicitly models a transient reflow rather than claiming to measure one.

Before the product repair, the new scenario fails its exact full-document
comparison: a requested `[18.75, -11.25]` instead moves every anchor by
`[18.75, 5.79375]`. With the origin captured before replacement, the same scenario
passes for both open and closed paths. It checks press/preview document
neutrality, exact rigid placement with every non-position field unchanged,
one separate placement Undo unit, Undo/Redo, Escape, pointer cancellation and
exact out-and-back. Existing ordinary-geometry assertions remain unchanged.
This establishes a real ordering defect under the modeled geometry change;
it does not establish that the original CI failure used that internal CTM.

The unchanged real open/closed placement chain still needs actual verification,
including cold JSON reopen and exact SVG/PNG output. Keep the internal-read
receipt until that evidence determines whether this ordering repair also
resolves the reported Windows failure. No browser rerun or native acceptance
is established by the DOM regression alone.
