# Luminance to Points

Preferred open composition for custom luminance patterns:

`Filled vector / PNG → Luminance to Points → Instance on Points: Points`

`Any Curve / Circle / Polygon / Star / compound Path → Instance on Points: Source → Fill / Stroke → Output`

The existing Luminance Instances node already combines sampling and motif
placement internally. It remains unchanged. The new node exposes its sampling
and linear white/black size/angle mapping as a reusable point Path. Existing
Instance on Points consumes optional per-point transforms through its existing
transform machinery; it does not need a second instancing engine or new socket
type. The named shape controls remain available for the existing Dither nodes;
native generators are reusable motifs in this open graph.

Each selected center is a disconnected, one-anchor open contour with zero
handles. Its ID is derived from producer ID and signed grid indices, independent
of array position. Evaluated point metadata carries
`instanceTransform: {scale, rotation}`, plus sampled `luminance` and remapped
`darkness`. The attributes are derived from saved node parameters; no sampled
geometry cache or image pixels are authored into the project. Connect directly
to Instance on Points: intermediate Path operations may discard point attributes.
The point Path is placement data; use instancing to preview/render its motif.

Spacing is center-to-center distance (1–1024 document px, default 32). Size is a
multiplier of the authored motif dimensions, independent of spacing: linear
`white + (black - white) * darkness`, default white=0 and black=1. Scale endpoints
are 0–100; angle endpoints are ±360 degrees, default 0. A computed zero size
omits that point. Changing a motif or its dimensions does not resample points.
Per-point rotation overrides Align Tangent, just as the existing explicit
per-instance transforms do. Ordinary target Paths without attributes retain
their exact previous translation/tangent arithmetic.

Sampling reuses existing linear-light luminance and alpha-over-white, followed
by Invert and Tone power. PNG uses nearest texels with the existing decode cap.
Centers must be in the source mask/image rectangle; holes and outside centers
are omitted. Motifs are complete paths and are not clipped to cells or the
source boundary. Spacing below motif extent allows intersection/overlap, including
connected black areas and white diamond gaps. No Union or smoothing is added.
Fill/Stroke applies after instancing. Existing independent groups preserve paint
alpha: opaque black overlaps visually connect; translucent motifs retain the
existing stacking behavior. The source image/paint and its alpha remain intact.

Limits stay at 4096 sampled cells and 256 nonzero point instances, with existing
mask work, motif/group, 10000-contour and 100000-point output budgets. Excessive
density rejects atomically; no truncation or raised budget is introduced. The
Inspector explains these limits. Filled vector masks must remain closed and
ungrouped; arbitrary open/cubic/grouped geometry is accepted as the motif.
Bypass is unavailable on the producer because paint and point output differ.

The procedural node promotes files to schema 36, shared with this task's Dither
controls. Existing documents without either feature keep their prior version.
Old Instance on Points and Luminance Instances outputs remain unchanged.
No new authored point-attribute editor, generic Field system or native package
is introduced.

Verification: `tests/luminance-points.test.js` checks stable IDs, independent
spacing/size, rotation and cubic handles, direct attributes, old arithmetic,
PNG alpha, schema/JSON rebuild, Undo/Redo, lock and atomic budget rejection.
`scripts/luminance-points-example.js` makes a synthetic editable native example,
SVG and cold-reopen comparison. `tests/luminance-points.e2e.js` checks production
UI controls, actual arbitrary-motif wiring, preview/SVG/PNG equality and saved
reopen. Generated evidence lives in ignored `test-results/dither-controls/`.
