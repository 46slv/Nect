# Adjacent graph duplication

Ctrl/Cmd+D with focus inside the graph or its toolbar duplicates the selected
nodes. The node button and header context menu use the same helper. Copies become
the selection, graph focus is retained, and the first copied card is revealed.
One Undo removes the complete copy; Redo restores it.

Placement uses unscaled measured card dimensions, including resized and expanded
cards. The selection's bounding rectangle moves right with a 24-world-unit gap,
skipping occupied rectangles. Relative positions within a selection stay exact.
This conservative placement can skip empty space inside a sparse group; it does
not rearrange existing nodes or change artwork geometry. Hidden/unmeasured cards
use the existing width or a type-based fallback. There is no clipboard dependency.

The existing Session `duplicate-nodes` command accepts an optional finite two-axis
`offset` relative to original positions, mutually exclusive with single-node
`position`. Legacy calls retain their placement semantics. Internal wires and
expression references remap through the existing duplication implementation;
external references remain external. Existing node limits, locks and edited Path
Remap/source restrictions remain authoritative and failures commit nothing.

Text/select/contenteditable controls, dialogs and other views retain keyboard
ownership. Shift/Alt variants are not duplication commands. Plain Ctrl/Cmd+D in
the graph consumes repeats and blocked states, preventing the browser bookmark
fallback there. IME, gestures, rename, modal edits and variable-font preparation
cannot trigger duplication. An empty selection does nothing.

Verification: focused layout/Session and synthetic-DOM tests cover placement,
group/reference preservation, invalid-offset rollback, one-step Undo/Redo,
Ctrl and Cmd, repeated invocation, auto-repeat, empty selection, collapsed cards,
active gestures, editing fields, other views, IME, dialogs and locked layers.
Synthetic DOM is not browser or native WebView interaction acceptance. Actual
scroll/reveal, high-DPI/zoom appearance and native key handling remain unverified.
No EXE build is part of this change.
