# Maintained character ink gaps: v1 implementation contract

Status: source implementation and focused automated verification on 2026-10-17. Browser/native visual acceptance remains pending; this does not claim production delivery. Transient dragging is specified separately in `character-alignment-snapping.md`; a completed guide snap writes ordinary offsets and does not maintain gaps after typography changes.

## Product boundary and smallest useful mode

The gap input accepts finite numbers from 0 to 100000 inclusive. Blank, browser-invalid, nonfinite and out-of-range input is rejected before changing the document or committing a pending Text draft, both on change and Apply/Reapply. The control restores the stored gap, or the initial 10 px when the mode is off, and reports the allowed range. Explicit `0` remains valid; clearing the field never requests zero spacing.

Add an explicit **Maintain equal ink gaps** action to one selected Text. Initial scope is the entire Text containing at least two mapped, nonempty glyphs on exactly one hard line (horizontal X), or one vertical column (vertical Y). Do not add this to every Text, implicitly enable it after snapping, or infer a constraint from coincident positions. No arbitrary subset/group constraint in v1.

The user supplies a nonnegative gap in **Text-local px** (document units, not CSS screen pixels), initially 10 px. Explicit Apply is required; preview can show the proposed arrangement. A gap control is sufficient; no automatic estimate is necessary. Bounds mean the actual outline's axis extrema, including per-character width/height/rotation/scale, but excluding strokes, effects and selection cells. This is equal geometric ink-box spacing, not optical spacing, side-bearing equality or baseline spacing. Curved/diagonal glyphs can still look uneven.

Use logical stable-ID source order, never current spatial order. Arrange left-to-right for `horizontal-tb`, top-to-bottom for `vertical-rl`. Crossed/manual positions are reordered only when the user explicitly applies the mode; do not silently change source text. Arbitrary physical axis selection, RTL/BiDi shaping, centered/end anchoring, multiple lines/columns, cross-Text distribution, Type on Path, optical kerning and fixed-total-span distribution are outside v1.

### Anchor and units

The first glyph's minimum ink edge is the only v1 anchor. On Apply capture its currently evaluated Text-local coordinate minus the evaluated Text Position on the progression axis. Persist that `anchorOffset`; thereafter the first ink minimum is `evaluatedPosition[axis] + anchorOffset`. Font/size/style changes hold this edge fixed. A Text Position edit or connection moves the whole arrangement. Existing Position OUT semantics stay unchanged: Position OUT is the Text reference, not the first ink edge.

Store px only, with `gap` in [0, 100000] and `anchorOffset` in [-100000, 100000]; do not infer em from whichever character is first/largest. A size change retains the numeric px gap. Em-relative gaps are a separate future mode that would need an explicit reference size for mixed-size text.

The constraint is applied before downstream transforms. All output instances share the same source arrangement. Rotation, nonuniform scale and shear can change displayed artboard-axis gaps, and shape-changing downstream nodes can change them further. The inspector must say “Text-local ink gaps; downstream effects may change visible spacing.” Do not promise artboard-space equality under arbitrary downstream transforms or solve separately per occurrence. This source mode does not require choosing an invertible occurrence. Canvas interaction keeps its existing route/affine eligibility checks.

## Unsupported and suspended states

Apply is unavailable with a specific reason when any of the following holds:

- No Text, locked target, fewer than two characters, missing/unloaded font or missing glyph, unsupported shaping/mapping, or nonfinite/too-complex outline.
- Any whitespace or empty-outline glyph, including leading/trailing spaces. Never treat a synthetic whitespace selection cell as ink and never close a word space silently. Text remains editable; remove the space or use normal tracking instead.
- Any hard newline, including a trailing blank line; any `textWrap` setting even if it currently yields one line; or a path attachment. Rejecting wrap avoids the layout/spacing/reflow dependency cycle.
- Character identity record with a mismatched normalized basis or layout mapping reason. Verify one-to-one correspondence between `characterPositions.units`, `layout.characters`, glyph source indices and IDs; do not allocate identities from a sorted outline.

When an already enabled arrangement becomes unsupported, retain its entire authored intent and character data, return ordinary layout without arrangement corrections, and expose a distinct runtime suspended reason. Do not silently drop IDs, shorten the run, delete the constraint or redistribute just the survivors. Font loading errors keep the existing evaluation error; do not substitute another font. Export must reject a suspended arrangement with its specific reason instead of silently exporting the fallback spacing. The editor can render the normal-layout fallback with a conspicuous status. The new runtime status must join existing export-safety checks explicitly; character-position suspension alone is insufficient.

Text insertion/deletion/replacement/reordering is allowed through the existing verified editor. Any change to the stored membership/order suspends v1, even if it remains a single line. An explicit **Reapply to current text** replaces membership and recaptures the first-edge anchor, preserving surviving authored adjustments. Undo can restore the old valid state. Identical canonical representation changes with the same IDs need not suspend. Replacing a selected character with the same letter but a newly allocated ID does suspend. No automatic enrollment of typed characters.

A restored compatible font, removed unsupported setting, or Undo may resume automatically only if the exact stored membership/order and valid mapping return. An invalid externally edited identity basis never resumes by positional guessing.

## Storage and identity

Proposed new optional Text-node property (not inside the existing characterPositions version):

```json
{
  "characterArrangement": {
    "version": 1,
    "mode": "equal-ink-gap",
    "axis": "x",
    "ids": ["c1", "c2", "c3"],
    "gap": 10,
    "anchorOffset": 0.3125
  }
}
```

`axis` is `x` or `y` and must match the writing mode at evaluation. IDs must be unique, valid existing character ID syntax, ordered, and have length 2..10000. Only the listed keys/enum values are accepted. A well-formed record whose IDs no longer match the current text is valid saved-but-suspended data; structural invalidity is rejected during import. Do not require every ID to exist at deserialization, because deletion is a supported suspended state. The identity record itself still passes its normal validation.

A finite `anchorOffset` plus current Position is enough; do not persist world coordinates, glyph widths, derived offsets, a second copy of font metrics or transient status. Absence means off; no enabled flag is needed. Clearing removes this property. Preserve the existing `characterPositions` version and sparse offsets/styles/transforms/kerning pairs. Applying to previously unadjusted text must create a stable identity record with `createCharacterPositions`, then use its units; never leave an arrangement dependent on fallback `cN` labels.

Important existing-source trap: `characterPositionStatus` only suspends a mismatched basis when `hasCharacterAdjustments(record)` is true. An arrangement can be the only adjustment, with all maps empty. Therefore arrangement evaluation independently requires `record.basis === normalizeCharacterText(text)`, a real record, and exact ordered ID coverage. Do not assume the current status helper protects this feature. `reconcileCharacterEdit` can allocate a new empty-adjustment record on unverified edits; the stored IDs must remain untouched and fail membership rather than be rebound to that replacement record.

The current document schema maximum is 23. Allocate the next free document schema version (24 if still free at implementation), whitelist/validate the new property only on Text, and update new-document/serialization/validation version gates consistently. Incoming older documents omit the property and must render and serialize with existing compatibility behavior. No migration enables the mode or manufactures arrangement metadata. Unknown arrangement versions fail clearly; old readers should reject the new document version rather than ignore its spacing intent.

## Exact evaluation seam and algorithm

Observed current flow:

1. `core.js` Text evaluation calls `layoutText` with evaluated Position/size plus node settings and font assets.
2. `text-layout.js` normal layout creates glyph command ranges, advances, kerning/tracking, hard/wrapped line spans, line alignment and cross-axis reference.
3. Near the end, each glyph receives its authored XY and rotation/scale around the normal advance-cell pivot. Its command slice then yields exact OpenType bounds and `characters[]` path/selection metadata.
4. Core optionally projects Type on Path, converts `layout.path` to Paper contours, records `textLayouts` and character occurrences, and continues downstream geometry evaluation.

Implement a small pure `applyCharacterArrangement` helper at the end of step 3, before final serialization/return from `layoutText`, or as a layout-owned finalizer called there. Pass the evaluated Position and arrangement intent from core. Reject path attachment at core before applying. Avoid implementing this in `main.js`, the drag solver, font asset loader, or downstream occurrence traversal. It must run in headless evaluation and export, not only when the inspector is open.

Two-pass layout finalization is necessary: obtain all fully transformed, unrounded outline bounds first; then translate each eligible glyph's exact command range. Do not compute fresh bounds from `characters[].path` (currently rounded to five decimals) when the original command ranges are already available.

Let ordered normal/transformed ink intervals be `[l_i, r_i]`, widths `w_i = r_i-l_i`, and `A = Position[axis] + anchorOffset`. Compute:

```text
s_0 = A
s_i = s_(i-1) + w_(i-1) + gap
delta_i = s_i - l_i
```

Translate all endpoints and Bézier controls in glyph i by `delta_i` on the chosen axis; never scale the outline. Use the original command ranges and fresh local bounds after all authored shape changes. Check every bound/correction/final coordinate for finiteness and keep existing complexity limits. Do not round successive spacing deltas, feed previous derived deltas into the next evaluation, or write them to the document. Adjacent final intervals then satisfy `left_(i+1)-right_i = gap`; the other axis is unchanged.

Update `layout.path`, `characters[].path`, and `characters[].bounds` together, before Paper conversion, or hit testing and export will disagree. Add runtime `characters[].arrangementOffset: [dx,dy]` while keeping `offset`, `base`, `pivot` as authored/normal-layout values. Update the character pivot overlay to draw `pivot + offset + arrangementOffset`; the current `main.js` overlay only sums the first two. Any future gizmo must use the same displayed pivot. Do not rewrite glyph advances, `glyphs.x/y`, lineOffsets, pairKerning or source indices to impersonate metric changes. Document that glyph x/y remain normal-layout provenance; output outlines/character bounds are final geometry. Runtime arrangement metadata exposes `{active, suspended, reason}` separately from ordinary character identity status.

No constraint means an exact early no-op: do not reserialize paths through another library or reorder commands for legacy documents. Preserve the current fallback/error behavior before introducing the arrangement pass.

## Precedence and escape behavior

- Font family/style, Text size, per-character size/width/height, rotation/scale and baseline shift remain editable. Evaluate their true resulting ink before spacing. Rotation/scale still use the original normal-layout pivot; apply the arrangement translation once afterward.
- Authored XY arrays remain byte-for-byte intact. Cross-axis offsets remain visible. Axial offsets are retained but compensated by the constraint, so they do not change the maintained gap. Explain this explicitly in controls rather than showing editable axial controls that appear broken.
- Tracking, automatic kerning, pair-kerning overrides and progression-axis Position reference continue to exist in normal layout, but their progression-axis placement is superseded by this first-edge arrangement. Do not delete or rewrite them. Disable their direct editing controls while active with “Maintained ink gaps control spacing; disable to edit.” If bindings change them, recomputation still maintains the constraint; label their placement contribution as overridden. Width/glyph shape changes remain effective. Cross-axis alignment and leading keep their normal-layout semantics; v1 has only one line/column.
- A writing-mode change changes the required axis and suspends the stored arrangement. Do not reinterpret an X anchor as Y. Explicit Reapply chooses the new axis and captures a new anchor.
- Disallow progression-axis character translation and unrestricted character drag while active. Cross-axis numeric offset edits, or an explicitly cross-axis-constrained drag, may proceed; do not silently strip the user's axial movement. Every command entry point must enforce this, not just disabled UI. Ordinary Text Position movement is permitted. Transient equal-gap snaps must not mutate active arrangement members through a bypass.
- **Disable maintained gaps** removes the intent and restores the normal layout with the original authored offsets at the current typography. Warn in the control copy that this may move letters. It does not bake current arrangement corrections into offsets. “Freeze current placement” would be a separately authorized future command because it necessarily changes authored offsets; omit it in v1.
- Reset offsets along the controlled axis is blocked while active unless preceded by explicit Disable. Reset all character adjustments should explicitly include clearing arrangement intent in the same undoable command; label that scope in the UI. Shape/style-only resets can remain active and recompute.

## Command and Undo contract

Add one atomic `set-character-arrangement` command with nodeId and either `{mode, gap}` (Apply/Reapply; derive axis/IDs/anchor from current validated evaluation) or a narrowly typed gap update retaining existing IDs/anchor. Add `clear-character-arrangement`. Do not accept arbitrary stale caller-supplied IDs or anchor positions from canvas state. Commands validate lock/layer/target, flush a valid pending text edit through the existing session flow, resolve current font data, evaluate candidate geometry, and commit only on success.

Apply creates an identity record if necessary and stores the intent in one history entry. Identical gap updates are no-ops. Gap scrubbing uses the existing begin/update/commit/cancel gesture ownership against a base snapshot: one Undo, Escape restores the complete base. Each ordinary font/style/size edit retains its existing one-Undo boundary; recomputation creates no secondary history entries. Undo/Redo restores exact intent, source text, identity record, authored styles/offsets, and rendered result. Disable is one entry and Undo re-enables it. Failed validation or cancellation is mutation-free.

Do not auto-disable on a pointerdown or first character drag. Require the explicit Disable action first, so cancelling a drag cannot leave a hidden arrangement change behind. Invalidating edits (newlines, wrap, path attachment, ID changes) remain one normal edit plus a visible suspended status; no additional generated command.

## Source-bound reproduction and focused acceptance

An isolated Node probe used current `layoutText`, `setCharacterOffsets` and bundled `public/fonts/DejaVuSans.ttf`. Position [100,200], text `AgW`, size 40, gap 10, anchor 100.3125:

- Normal X intervals: A [100.3125,127.03125], g [129.5703125,149.12109375], W [154.08203125,190.99609375]. One-time offsets [0,7.4609375,12.5] produce both gaps 10.
- Keeping those offsets and changing size to 80 gives gaps **12.5390625 and 14.9609375**, so even equality is lost. Fresh derived placement at the saved anchor gives [100.3125,153.75], [163.75,202.8515625], [212.8515625,286.6796875], restoring both gaps to 10 without changing the text.

These measurements reproduce the limitation and verify the formula; they are not tests of an implemented maintained mode. Keep acceptance focused in new pure/layout/session tests, without running the full suite for this design task:

1. `AgW` above, size 40 → 80 → 40; exact anchor and 10 px gaps (1e-7 unrounded bounds tolerance; exported/serialized path tolerance at least 2e-5). Repeat a single-character size/width/height change, mixed family/style via existing font assets, and a rotated/scaled glyph. Repeated evaluation is drift-free.
2. Use existing `tests/fixtures/fonts/NectJPFixture-Regular.otf` with `日A` in vertical-rl (Latin A can use bundled DejaVu via `setCharacterStyles`). Assert top anchor/Y gaps after per-character size/style changes, with vertical substitutions/sideways geometry taken from real layout. Cross-axis values remain unchanged by the derived pass. A real font-style switch test must use actually loaded alternate bytes; do not claim a synthetic font ID change proves family/style support.
3. Preserve serialized offsets, transforms, tracking, kerningPairs, text and IDs through Apply/size edit; verify they are exactly restored after Disable. Existing nonzero cross-axis offsets and baseline shifts remain visible. Active axial writes are rejected atomically through direct commands and canvas paths.
4. JSON round-trip, schema gates, malformed/unknown intent rejection, old document no-op, repeated letters, canonical text normalization, same-value replacement with new IDs, deleted/inserted characters, record-basis mismatch with empty adjustment maps. Never guess IDs.
5. Newline, spaces, whitespace-only glyph, wrap, path attachment, writing-mode change, missing glyph/font, mapping failure, overflow/nonfinite corrections: correct disabled/suspended reason, retained intent, no partial arrangement. Suspended export fails explicitly; restoring exact eligibility resumes.
6. One Undo/Redo per Apply, Disable, gap scrub and typography edit; no-op/cancel/failed Apply adds no history. Reset-all explicitly clears intent atomically.
7. Output path, character overlay bounds, displayed pivots and exported outline agree. Text Position movement translates arrangement; downstream affine instances share the source layout and do not trigger per-instance reflow. Browser/native visual acceptance remains a separate later check.

This v1 answers editable single-line/single-column maintained spacing. It deliberately does not promise uniform optical typography, preservation of word spaces, multi-line layout, arbitrary downstream artboard gaps, or persistence from ordinary transient snap alone.

## Implementation evidence (2026-10-17)

- `src/character-arrangement.js` validates optional schema-24 intent and applies a two-pass translation to fully transformed original command ranges. Exact unrounded bounds drive deltas; authored offsets, pivots, advances and typography are untouched. Absence is an early geometry no-op.
- `text-layout.js` exposes `arrangementEligibility: {eligible,reason}` and separate `arrangementStatus: {active,suspended,reason}`. Active characters expose `arrangementOffset`. Core includes intent in the evaluation cache key and transports runtime status through character occurrences to export safety.
- Commands: `set-character-arrangement` with `{layerId,nodeId,mode:'equal-ink-gap',gap}` applies/reapplies from current validated displayed geometry. `{layerId,nodeId,updateGap:true,gap}` retains membership and anchor. `clear-character-arrangement` removes intent. Arbitrary caller membership/anchor is rejected. Apply atomically incorporates a valid pending Text draft. New documents use schema 24; old files retain their existing version unless a feature requires promotion.
- Gap gesture target: `{kind:'text-values',textKind:'arrangement-gap',layerId,nodeId}`, with ordinary session `update:{values:[gap]}` ownership, preview revision and commit/cancel. Commands and numeric/drag gestures protect the active axis, pair kerning and direct tracking changes. Cross-axis movement and Text Position edits work. Reset all removes arrangement; resetting selected typography can clear its overridden tracking.
- Suspended membership/order, basis, whitespace, wrap, attachment and writing-mode intent is retained. Export and outline baking reject suspension with its reason. Valid outline baking freezes arranged output and detaches intent with the Text-to-Multi-Path replacement.
- `tests/maintained-character-spacing.test.js`: 12 focused tests cover exact DejaVu size 40→80→40 gaps/anchor, drift, transformed/styled ink, vertical CJK fixture plus actually different DejaVu/CJK font bytes, schema validation/round trip, suspension/restoration, same-value replacement with new IDs and explicit reapply, direct/gesture axis guards, Undo/Redo, gap cancel/no-op, export/bake, cache invalidation, missing-font refusal, locks and atomic draft flush. No new font asset was added.
- The adjacent character-position and character-transform focused tests also pass. One pre-existing geometry fixture incorrectly forced schema 4 after creating a modern expandable Join; it now keeps the current schema. Dedicated authentic schema-4 round-trip/rejection coverage remains unchanged.
- No full test suite, browser/native acceptance or application build was run by the core implementation worker. UI integration and final web build are separate integration checks. Optical spacing, whitespace preservation, multiline/reflow, arbitrary downstream artboard gaps and variable-font support remain outside this feature.
