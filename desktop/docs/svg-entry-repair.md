# SVG entry and initial Composition

Opening a project now selects a visible, unlocked Composition with a connected
Output when available. Empty editable, visible and first Composition fallbacks
follow in that order. This changes editor selection only; source order, Hide,
Solo, locks and references remain authored as before. Explicitly selecting a
locked Composition and opening Add node reports the lock instead of returning
silently.

Add SVG checks its target before opening a file chooser. The same shared check
also protects the common `add-svg-part` command. It requires an unlocked visible
regular path Output; an Image Output is reported as an incompatible target.
This action appends a vector part and does not implicitly replace an image.

Opaque same-color unnamed rectangle batches now separate touching/overlapping
axis-aligned bounds into different paint batches. Combining touching closed
contours into one fill loses independent edge coverage. Reordering is limited to
one consecutive identical opaque solid-paint bundle; gradients, transparency,
strokes, names and other paint boundaries remain separate. Bounds use the parsed
geometry. Existing source/geometry budgets remain; each batch is below90KB and
one bundle permits at most64 paint batches. No rendering or geometry limit is
silently bypassed.

`svg-rectangle-coverage.e2e.js` compares the new batches against independently
painted cubic paths and the previous touching compound. Native SVG rounded-rect
primitives can still antialias differently from cubic paths; this is not a claim
of universal pixel equality. The private attached QR replacement is validated
separately, including matching local decoded payloads and retained quiet zone;
its input, design, font assets and generated document are excluded from Git.

Evidence: `initial-layer.test.js`, `svg-rectangles.test.js`, `svg-part.e2e.js` and
`svg-rectangle-coverage.e2e.js`. A separate profile running the same distributed
a85aa74 EXE reproduced the locked first-Composition state, successfully added and
undid a node after choosing an editable Composition, and completed actual native
font IPC without JS errors. The user's running process was not closed, attached
to a debugger or modified. OS file-chooser interaction, manual IME and high-DPI
acceptance remain distinct from these checks.
