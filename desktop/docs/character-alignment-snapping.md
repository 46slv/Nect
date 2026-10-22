# Character ink alignment and equal-gap snapping

Transient snapping is implemented for character dragging within one Text and
one supported output occurrence. It aligns visible ink edges/centers and
adjacent equal gaps. It does not persist an arrangement after font/size edits,
perform optical kerning, or align characters from other Text nodes.

## Geometry and identity

`src/character-snapping.js` snapshots exact artboard-space Paper curve bounds
from evaluated glyph outlines after local character and downstream affine
transforms. Transforming the corners of a local axis-aligned box is not an exact
ink bound under rotation or shear; Bézier extrema are required. The existing
outline serialization uses five decimals, so numerical checks allow the
corresponding tolerance rather than rounding every drag delta.

Whitespace selection cells remain useful for hit testing but never become ink
anchors. Empty/nonfinite paths, suspended mappings, hidden layers, unsupported
routes and singular transforms supply no candidates. `sourceIndex` is a
normalized layout-unit index, not a UTF-16 offset or an identity. Character IDs
and occurrence provenance must come from the evaluated mapping. No IDs are
inferred from glyph order, contour count or spatial sorting.

Selected visible glyphs provide left/center/right and top/center/bottom anchors.
Unselected glyphs in the same Text and chosen occurrence provide fixed targets.
All moving IDs are excluded. Other displayed occurrences of the same Text are
also excluded because changing source offsets moves them too. One correction
moves the whole selection rigidly; selected characters never snap independently.

## Solver and gesture contract

`src/main.js` captures geometry once after character selection is accepted and
focused text drafts are resolved. Subsequent moves validate selection, route
and matrix. There is no per-move font loading, fresh layout evaluation or Paper
candidate rebuild. A new drag after a typography change uses fresh geometry.

`src/guides.js` accepts an optional final candidate list in its existing solver.
Character candidates participate in the same nearest-correction search as other
targets, never a second snapping pass. Exact-distance precedence is explicit
user guide, character edge/center, character gap, guide layer, artboard, artwork;
grid is fallback. Within character alignment, stable source ID/anchor followed
by stable target ID/anchor breaks ties. Gap construction order is between
neighbors, preceding pair, following pair.

Movement remains initial artboard pointer delta → snap correction →
`inverseCharacterDelta` → Session `character-translate` gesture against original
offsets. This preserves pointer anchoring, nonzero offsets, rigid multi-selection
and downstream rotation/reflection/nonuniform scale. The tolerance is
`6 / canvasScale()` document units, equal to six CSS pixels at every zoom.
Global Snap, Alt bypass and forbidden axes apply to every candidate. Character
targets do not depend on the unrelated Snap artwork preference.

Physical X/Y remain the alignment axes in horizontal and vertical writing.
Evaluated vertical substitutions, punctuation and sideways Latin provide their
actual outlines. Baseline, advance, leading, font metrics, kerning, size and
position-reference settings are not changed by snapping.

The existing three-CSS-pixel drag threshold, Shift-click membership, Shift-drag
axis lock, overlap chooser, stage focus, pointer capture, locks and visibility
checks remain. Stale or unsupported provenance cancels instead of applying stale
geometry. One completed Session gesture gives one Undo. Escape, pointercancel,
route invalidation or target loss restores the base and clears guides. Clicks,
no-ops and cancelled gestures add no History entry. Text input/contenteditable
and composition retain their caret/range/IME ownership; this helper does not
intercept direct text editing, pan or outline editing.

## Bounded equal-gap assistance

The selected glyphs must form a contiguous rigid block in one evaluated
source-index line/column. Ink intervals must be ordered and nonoverlapping;
crossed/interleaved runs suppress gap candidates but not ordinary edge/center
alignment. The gap axis is the dominant displayed axis of the occurrence's
writing-direction vector, with X winning a diagonal tie. Reflected order is
supported. Whitespace, other lines/columns and other occurrences cannot provide
gap templates.

For moving interval `[m0,m1]` between stationary neighbors `[l0,l1]` and
`[r0,r1]`, the centering translation is `(l1 + r0 - m0 - m1) / 2`; both final
gaps must be nonnegative. An adjacent stationary-pair gap `g` can also be
matched at either end: `t1 + g - m0` after a neighbor, or `t0 - g - m1` before it.
Candidates must preserve the intended adjacency. At most three candidates are
built: between immediate neighbors, immediately preceding stationary pair, and
immediately following stationary pair. Arbitrary glyph-pair searching is not
part of this contract.

A winning gap candidate draws two short dimension brackets in artboard
coordinates, positioned from the final ink bounds. These transient paired marks
distinguish spacing from ordinary full-length alignment guides and never enter
exported SVG or authored geometry. Global off, Alt, end and cancel clear them.
Geometrically equal ink gaps are not a claim of optically equal spacing.

## Focused verification

The recorded implementation check passed 21/21 focused tests from `desktop/`:

```sh
node --test tests/character-snap-math.test.js tests/character-snapping-dom.test.js tests/axis-drag-dom.test.js tests/guides.test.js tests/artwork-snap-dom.test.js
```

Coverage includes transformed cubic extrema (141.421356… rather than the
incorrect transformed-box bound 175), deterministic ties, 5.9/6.1-CSS-pixel
threshold, global/Alt bypass, axis exclusion, rigid selections, between/either-end
gaps, vertical columns, excluded whitespace/other lines/overlap/mapping, fresh
typography, mixed fonts/sizes, rotated/sheared/reflected transforms, zoom/pan
guides, descenders, paired brackets, locks, Undo/Redo and rollback.

The DejaVu Sans/Nect JP Fixture comparison used 40/70 px text, 17° local rotation,
1.2 local scale and downstream `[1.2, 0.5, -0.4, -1.8, 80, 30]`. Reevaluated
horizontal `Ag` and vertical `A、` ink-bottom residuals were below 0.000012
document units; the tests allow 0.00005. A gap fixture with stationary
`[0,10]` / `[70,100]` and moving `[25,45]` snaps translation 3 to 5, yielding
two gaps of 20.

The corresponding Web build passed (88 modules). No full suite, native build,
browser automation or physical/native pointer acceptance was run for that
implementation check. A separately attempted legacy
`character-position-dom.test.js` stopped at a pre-existing schema expectation
(3 versus then-current 23); it was not changed or counted as passing. The focused
snapping DOM test is schema-independent. This documentation update does not
rerun those product checks.

## Separate future behavior: maintained spacing

A completed snap authors ordinary XY offsets. It does not maintain gaps when
font family/style/size/width changes. Persistent spacing would require an
explicit stable-ID arrangement intent, fresh bounds after layout, a defined
anchor and clear precedence with manual offsets. Each arrangement or later
typography edit would still need one atomic Undo and JSON round-trip support.

Unresolved semantics include Text-local versus displayed-artboard distance,
multiple transformed occurrences, line wrapping, insertion/deletion/reordering,
baseline shifts, individual scale and fixed gap versus fixed total span.
Unsupported mapping must suspend rather than detach identities silently.
Type on Path and divergent shared-output transforms need their own contract.
These are future design requirements, not implemented or passing acceptance.
