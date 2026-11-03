# Character and typography numeric dragging

## REQ-42 P1 common behavior

The first consolidation slice connects the ordinary Circle **Radius** scalar and
owned-point **ΔX / ΔY** to `numeric-control-behavior.js`. The behavior handles the
4px activation threshold, the existing `scrubNumericValues` calculation,
target/revision revalidation, cancellation and no-op completion. It owns no model
or history. Other numeric surfaces retain their existing implementations pending
their own migration.

- Radius keeps the existing Session parameter gesture. Its preview is detached;
  a changed gesture commits one Undo. Pending typed input remains a separate edit.
- Point Δ edits its existing draft and peer controls only. Pointer release does
  not Apply. The existing Move points button or Enter applies both axes in one
  Session command and one Undo. Enter during an active scrub cannot Apply early.
- Click and native typing/arrow keys remain available. Escape during a scrub,
  pointer cancellation, capture loss and window blur restore the exact prior
  staged strings. No-op and return-to-origin preserve Redo and authored state.
- Selection, source epoch, Composition, layer, Session revision, lock, connection,
  expression, disabled/read-only state and connected control identity are checked
  before preview and completion. A stale edit cannot retarget current selection.

| P1 binding          | Existing authority                                                                            | Operation metadata retained                                                                                         |
| ------------------- | --------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------- |
| Circle Radius       | Existing Scalar parameter and Session validation; `numericLimits` remains the UI bounds owner | Existing 1 displayed unit per screen pixel, finite sentinel bounds, native number keyboard behavior; no new min/max |
| Owned-point ΔX / ΔY | Existing `pointDeltaDraft`, selected-point guards and `translate-points` validation           | 1 document unit per screen pixel, finite delta, no new geometric bounds; Apply retains the captured revision        |

Display strings are not committed merely to normalize formatting. No UI metadata
is added to `definitions`, Parameters or serialized documents. P2 Document/Guide
unit conversion and Text mixed selection, and P3 remaining/async surfaces are not
part of this slice. Chrome acceptance and Windows WebView2/physical IME acceptance
remain separate evidence.

## REQ-42 P2a Document and Guide bindings

Document width/height and Guide position, grid spacing and grid angle use the
same input behavior as P1. Their existing Session numeric-command targets retain
command validation and history ownership. Document scrubbing calculates in the
selected px/mm/cm display unit; its baseline is the exact saved px value and only
the calculated delta crosses the unit conversion boundary. Guide fields retain
their existing document units and angle degrees.

Pending typing is settled as its own edit before the gesture snapshot. A changed
gesture is one Undo; no-op, return to origin and cancellation preserve authored
values and Redo. Unit/Composition/session/revision/target changes, disconnected
controls and Guide deletion/locking cancel rather than retargeting late input.
Completed dimension controls are settled before blur; an untouched formatted
display is not a new dimension command. Native selected-text drag/drop is blocked
only while the shared behavior claims that number field.

P2b Text mixed-selection and P3 remaining/async surfaces are not part of P2a.

## Existing character and typography coverage

The floating character palette's **ΔX**, **ΔY**, **Rotate °** and **Scale ×** inputs now use the same gesture lifecycle as graph/Inspector numeric inputs. Their Inspector copies are supported too. The original defect had two parts: the character inputs lacked the scrub opt-in, and the generic pointer handler assumed every opted-in input was a graph parameter inside a `.field[data-node]` wrapper. The floating palette has no such wrapper. Explicit Text/character target resolution now connects the real number inputs to model previews and the existing document transaction.

## Interaction contract

- Press inside the number, then move horizontally. A 4-screen-pixel dead zone preserves clicking and small hand jitter. The native right-side spinner hit area keeps its normal behavior.
- XY, rotation, size, tracking, baseline shift, manual leading and Wrap width advance by 1 displayed unit per screen pixel after activation. Uniform Scale advances by 0.01×. Selected horizontal width and vertical scale advance by 1 percentage point, converting to stored multipliers only when writing the model.
- Screen-pixel sensitivity stays the same across graph/artboard zoom. Existing numeric Shift/Alt behavior is unchanged; these inputs do not participate in artwork axis constraints or snapping.
- Click still focuses the field for typing and keyboard arrows. Typing applies one absolute value to all selected characters. A scrub adds a common delta to the original values, preserving differences in mixed selections. “Mixed” stays visible. The first selected value to reach a bound stops the group so their differences are retained.
- The floating palette stays in place while its number is being dragged, although the artwork previews live. It repositions after the gesture.
- A completed changed gesture produces one Undo. A pending typed value commits first as its own edit; the subsequent drag can be undone separately. No-op motion, returning to the origin and movement already blocked at a bound add no Undo, sparse overrides, character records or schema upgrades.
- Escape, pointercancel, lost capture and window blur restore the exact pre-gesture document and visible value, including a blank Mixed field. Late pointer events cannot revive the gesture. A different pointer cannot move or commit it.

## Supported fields and bounds

| Fields                                     | Display units      | Bounds           |
| ------------------------------------------ | ------------------ | ---------------- |
| Character ΔX / ΔY                          | local px           | −100000 … 100000 |
| Character Rotate                           | clockwise degrees  | −36000 … 36000   |
| Character Scale                            | uniform multiplier | 0.01 … 100       |
| Selected font size                         | px                 | 0.01 … 100000    |
| Selected horizontal width / vertical scale | percent            | 1 … 1000         |
| Selected tracking / baseline shift         | px                 | −100000 … 100000 |
| Manual leading / Wrap width                | px                 | 0.01 … 100000    |

Character edits retain Text Position/Position OUT, downstream transforms, unselected characters and unrelated properties. Selected typography still has its existing layout/reflow semantics. Character changes remain source-local under downstream rotation/reflection. Local numeric edits remain possible when the source mapping is valid but the output route does not allow canvas character manipulation.

The input must remain connected to the DOM, editable and enabled, and the same document layer/source/character selection must remain active. Composition locks and suspended character correspondence refuse edits. The existing graph parameter guards for locks, connections and expressions remain in force; a locked/driven Text Position does not lock its independent character overrides. Invalid layout/path/offset previews roll back to the previous valid preview. These checks apply before preview and before commit.

## Verification

- `tests/numeric-scrub.test.js`: threshold, units, finite inputs, mixed common-delta bounds and decimal/precision no-ops.
- `tests/character-numeric-scrub-dom.test.js`: actual rendered floating/Inspector fields, per-field and mixed edits, typography units, history/no-op/sparse record identity, pending draft, interruptions, readonly/disabled/detached protection, suspended correspondence, locked composition, leading and wrap width.
- `tests/character-numeric-scrub.e2e.js`: actual Chrome mouse hits on the input elements, both palettes, mixed typography, click/type/native arrows, interrupted gestures, pinned palette, zoom/modifiers and JSON/SVG persistence. Included in `npm run test:e2e`, with `test-results/workflow-character-numeric-scrub.png` as browser evidence when executed.
- Existing numeric-scrub, character-transform, rich-text, text-wrap and Shift-axis suites guard previous semantics.

Synthetic DOM results do not prove browser rendering. Record actual Chrome evidence for the exact candidate and distinguish local/CI execution and any route restrictions. Native WebView2 acceptance remains separate.
