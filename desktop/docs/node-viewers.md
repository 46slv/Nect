# Two live node Viewers (bounded first implementation)

Select one graph node. In Inspector, choose its public output when more than one exists, then press **Viewer 1 ←** or **Viewer 2 ←**. Both slots are live; selecting another node changes the Inspector without rebinding either slot. **Artwork** restores the existing final-output canvas. Assigning a Viewer never invokes **Use as output**.

Each slot shows node, port, type, composition and status. Path is drawn with an inspection-only neutral outline; PaintedPath and Image use the existing renderer and their own stage appearance. Scalar, Vector2 and Color use typed read-only text cards. Unassigned, missing identity/port, valid empty geometry and evaluation/asset errors are distinct. Missing identities remain bound and can recover through Undo. Clear removes the binding. Gradient IDs are namespaced per slot.

Wheel pans independently; Ctrl/Cmd/Alt + wheel zooms about the viewport center. Fit is explicit after the initial geometry fit. Reassignment retains the current view. Viewer pointer/keyboard focus cannot route deletion, nudges or tool shortcuts into the graph/artboard. Inspector edits remain normal document commands. No 1/2 keyboard shortcut is installed, so text/IME input is not intercepted.

Bindings, views and Artwork/Viewers mode are local editor state, outside Document/CommandSession/history. Open/New replacement resets them even if IDs are reused; composition changes retain stable-ID bindings. Nothing is persisted in project JSON. Evaluation is synchronous and updates through existing render and gesture-preview paths; no asynchronous result cache or whole-graph observer is added. Shared evaluations are reused within one refresh. Existing evaluator-wide structural errors can still affect a slot.

## Evidence

Focused command:

    node --test tests/node-viewers.test.js tests/node-viewers-dom.test.js tests/text-outline-path-ui-dom.test.js tests/blank-artwork-dom.test.js tests/math.test.js

Covers typed/public-port results, paint/image stage semantics, missing/empty/error states, live geometry edits, gradient namespace isolation, explicit port selection, selection independence, wheel state, read-only Delete, Artwork return, serialized document/SVG/history/dirty invariance for viewing operations, replace reset and existing selected-point/blank-artwork/math regressions. Web build is a separate check. Synthetic DOM tests do not establish pixel layout, PNG equivalence, production browser interaction or Windows acceptance.

## Deferred

Synchronized views, single-pane mode, drag panning, graph badges/context menu, shortcuts, source-edit/back navigation, snapshots/wipe, view-state persistence and Viewer canvas editing are outside this increment. No schema/core/Output changes. Native EXE and real-browser/manual visual acceptance are not performed here.
