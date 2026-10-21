# Text wrap width: center/end Position references

Base: verified PR53 `codex/text-wrap-canvas-resize`, remote
`61941a1865d4703d53670de577a44a12c962d1cd` in `46slv/Nect`.
PR53 rejects non-start alignment; there is no equivalent center/end width handle.
This branch extends that controller without another Text or frame schema.

Source contract: `text-layout.js` alignment factor start=0, center=0.5, end=1
anchors each line's advance, or visible outline when `referenceBounds=visible`,
to the unchanged Text Position. `main.js` labels this Position reference X;
it is not a second paragraph-alignment setting. Wrap width is the existing
advance budget. Width changes keep Position fixed and use existing reflow.
The numeric width setting and its evaluated SVG are the acceptance oracle.

Logical frame: left = Position.x - width \* alignment factor; right = left+width.
Start uses its existing right handle. Center also uses the right handle and
width = twice the signed distance to the fixed center reference. End uses a
left handle and width = signed distance from that handle to the fixed right
reference. The center frame expands symmetrically; the end frame expands left.
This frame is a width-budget guide, not measured ink containment or per-line
glyph bounds. Preserve both advance/visible reference modes and all authored
Text settings. Cross-axis scope remains origin/start; center/end Y is deferred.

Capture the reference along with the existing node/session/revision/selection
identity. Invert Canvas CTM and supported target affine; width delta factors are
start +1, center +2, end -1. Clamp to existing 0.01-100000 range on anchor
crossing; do not flip the reference or the handle side. Zero width change stays
a no-op. Reuse RAF preview, existing wrap Session gesture and final commit:
one drag/one Undo, exact Escape/pointercancel/blur restoration and stale rejection.
No change to text, character IDs, Position, selection identity, transforms or
connection topology. No core/model/schema, general resize, height/threading,
vertical, multiple-Text, rotated/skewed/reflected or driven/locked expansion.

Focused model/DOM and native synthetic tests compare smaller/larger widths to
numeric editing for center/end, advance/visible, supported scale/translation and
two Canvas zooms. Check frame reference, clamp/no flip, Undo/Redo, cancellation,
JSON/SVG and selection, plus the start-reference regression. Preserve prior
sealed candidates. Physical input, actual OS focus loss, OS dialogs/cold reopen
and IME remain separate unverified coverage; no full audit or Library upload.

Known inherited failures (PR52/53 baseline proof already retained):
`numeric-scrub-dom.test.js:119`, preview 500 vs expected480, is a pre-existing
numeric scrub preview expectation discrepancy; exact cause remains unclassified
between fixture and product, not a demonstrated regression from this branch.
`text-wrap-dom.test.js:82`, schema24 vs expected8, is an obsolete exact schema
assertion against the current document baseline, not evidence that resizing
changes schema. Leave both tests intact and preserve their earlier evidence.
