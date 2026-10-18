# Text outline baking and Multi Path

## User action

“Bake text to paths” is an explicit destructive edit with one-step Undo/Redo. It replaces the selected Text node, in place, with a `Multi Path` node. The ID, graph position, display name, width, collapse state, and outgoing Path connections remain intact. The command freezes the Text node's current evaluated output only. Downstream Fill, Stroke, Transform, Path Remap, and other modifiers remain live and unchanged.

Text content, typography, font references, character-edit provenance, upstream numeric connections/expressions, and Type on Path attachment are removed from the replacement. Existing font assets are deliberately retained in the document for other text and undo; this action does not delete or clean up assets. The baked geometry itself needs no font lookup. The existing `Text to Path` pass-through retains its existing behavior.

The existing SVG exporter already emits outlined vector paths. Baking makes the editable Nect document font-independent as well. This feature does not implement native Illustrator `.ai` files.

## Retained geometry

Schema version 22 adds `Multi Path`:

- `params: {}`
- `points`: ordinary editable point records, with stable IDs, Vector2 position/relative handle parameters and handle type
- `contours`: `{ id, closed, group?, points: [pointId, ...] }`
- `units`: `{ id, contours: [contourId, ...] }`

Every point belongs to exactly one contour and every contour belongs to exactly one unit. Existing contour geometry, order, orientation, closure and paint group are preserved exactly at the precision of the evaluated Text output; baking performs no additional rounding or flattening. IDs derive deterministically from the existing evaluated outline IDs. Text output already uses five-decimal OpenType path conversion.

Units are derived from layout glyph command ranges, including ligature glyphs. A glyph's outer outline and counters belong to the same movable unit. Units are separate from paint groups: adding paint groups would change overlap fill semantics. If command-range contour partitioning cannot be proven, all contours are conservatively kept in one unit. No contour is guessed to represent a letter. Whitespace has no outline unit. Units retain no editable characters or font IDs.

Multi Path has a normal Path output and ordinary point/handle editing. Existing downstream Remap fingerprints are preserved using the same geometry signature contract as Text. Moving/changing source geometry can therefore suspend old Remap edits, as before. Node duplication remaps points, contours and units together. Whole-artwork movement and duplication support the existing simple Multi Path → Fill → Output route, optionally with one terminal Transform. Direct Multi Path movement uses the 10,000-point editable geometry cap; the pre-existing Curve-specific 128-point movement cap is unchanged. Complex modifier routes retain their existing safe refusals. Copies remap all point/contour/unit links independently.

## Commands and gestures

All commands use the standard CommandSession revision/session envelope.

- `bake-text-outline`: `{layerId, nodeId}`
- `translate-path-units`: `{layerId, nodeId, unitIds, delta: [dx, dy]}`. Every anchor in selected units moves once; relative handles are unchanged. This is one undo step.
- Unit drag gesture: `{kind: 'path-units-translate', layerId, nodeId, unitIds}`; updates `{delta}` are absolute deltas from the gesture base, not incremental accumulations.
- Ordinary `curve-point` gestures, numeric point parameters, `nodes-translate`, `set-point-mode`, and `delete-point` accept Multi Path. Adding a point directly is intentionally unsupported; use Subdivide Path.
- `create-point-handles`: `{layerId, nodeId, pointId, delta?}` works on Curve, Multi Path, and Path Remap. Without delta, zero handles receive a neighbor-derived symmetric tangent. Existing nonzero handles are unchanged. With delta, Handle B is the requested relative vector and Handle A is its negative.
- Alt-anchor handle gesture: `{kind: 'point-handles', layerId, nodeId, pointId}`. Update `{}` for click initialization; update `{delta}` for pull-out drag. Commit creates one undo step and cancel restores the original. Path Remap stores the necessary deltas from upstream handles.

## Transactional refusals and limits

Baking refuses non-Text selection, locked compositions or Text parameters, disabled Text, unavailable/missing fonts or glyphs, invalid/cyclic expressions, suspended character adjustments, and empty/whitespace-only outlines. Valid driven numeric inputs are evaluated and detached. Consumers of Text Position or Text parameter references must be disconnected explicitly first, including Reference nodes; no dangling numeric link is silently created.

The full candidate validates before commit, including all graph references and the existing project byte limit. Baked geometry is capped at the existing editable-node limit of 10,000 vertices, with at most 10,000 contours and units. Unit moves reject any selected driven/locked anchor atomically. Handle creation rejects locked/driven anchor/handle controls and suspended Remap edits. No document or history mutation survives a failure.

## Verification

`node --test tests/text-outline-baking.test.js` covers real DejaVu outlines/counters, exact before/after SVG, reopen without supplied font, Undo/Redo, connected/expressive inputs, downstream transforms and Remap edits, vertical/rich/wrapped text, Type on Path, unit movement, ordinary point editing, Alt handles on Curve and Remap, gesture cancellation, locking/error/reference refusals, geometry caps and topology validation, and node duplication.
