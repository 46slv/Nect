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

## REQ-42 P2b Text mixed-selection binding

Selected-character offsets, rotation, scale, size, width, height, tracking and
baseline shift now resolve through the same input behavior as P1/P2a. They retain
the existing Session `text-values` gesture target and its validators. The binding
captures the exact character IDs, output route, source text, Composition, layer,
Session revision and control units before admitting a gesture. A change in those
guards cancels rather than applying late pointer input to another selection.

The calculation uses displayed units. Width and height remain percentages in the
input, then convert to saved multipliers at the Session update boundary. Pending
absolute typing is compared with the committed displayed values before capturing
the scrub baseline. Previously the legacy comparison divided the pending percent
by 100 while comparing it with displayed percentages, so typing 125% then dragging
committed 125% but cancelled the relative gesture.

Scrubbing adds a common delta to mixed values; typing sets the same absolute value
on selected characters. Preview stays detached from document/history, a changed
gesture is one Undo, and pending typing is a separate Undo. Cancel/no-op/return to
origin keep Redo, sparse records and inherited values. No calculation kernel,
Session command, schema, saved UI metadata or typography meaning changes.
Kerning, font selection/async work, wrap/leading and remaining surfaces keep their
existing routes for P3. Native WebView2 and physical IME acceptance remain separate.

## REQ-42 P3a Grid opacity

The Grid opacity number field now resolves to the existing `set-guide-setting`
numeric-command gesture. It previously had scrub affordance but no numeric target.
The adapter works in 0–100 percent and converts to the existing 0–1 saved value.
The initial canonical fraction is captured directly; an unchanged display never
round-trips it through percent conversion. Native range input keeps its existing
preview/change route. Both controls and the overlay share `previewGridOpacity`
and cancellation restoration; preview never changes the document or history.
Peer preview retains the typed percent string instead of regenerating it from
the saved fraction (for example, 29 must not become 28.999999999999996).

Pending typing is its own command before the one-Undo scrub. Cancel, no-op and
return to origin preserve Redo. Captured Session revision, Composition, control
identity and units reject late input. The common pointer handlers, calculation,
Session/core, schema and validation rules are unchanged. Other P3 surfaces and
native WebView2/physical IME remain separate work.

The input behavior optionally accepts
`normalizeValues(values, { baseline, bounds, requestedDelta })`. Its context
contains copies of the raw display baseline/bounds and the screen-pixel delta
before kernel bounding/rounding (the existing 4px dead zone and integer motion
times the binding step). The hook must return a same-length finite array. The
normalized values participate in equality, preview, value tracking and finish's
no-op decision. A refused normalization retains the previous accepted preview.
Bindings without the hook keep the existing calculation and completion path.

Only Grid opacity opts in. A zero request returns the raw display baseline.
Requests reaching or passing `min - baseline` / `max - baseline` normalize to the
exact endpoint, so high-precision fractions reach exact 0%/100% and saved 0/1.
Interior results keep the existing kernel calculation, including legitimate tiny
positive values. This is request-based normalization, not an epsilon clamp.
The shared kernel's rounding and mixed-delta policies are unchanged; untouched,
cancelled and return-to-origin edits retain the exact original saved fraction.

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

Manual leading and wrap extent on static Text use the shared behavior as node-level edits without requiring character selection. Wrap extent is width for horizontal text and height for vertical text. Their existing storage domains remain positive and may include values below the UI scrub minimum. Variable-font Text continues on its existing route because its preparation owner is asynchronous; migration of that route remains open.

Static Text pair kerning uses the shared behavior with its ordered character-pair identity and existing Session gesture. Inherited kerning remains distinct from an explicit zero; cancelling or returning to the origin preserves inheritance. Variable-font pair handling remains on its existing route.

The input must remain connected to the DOM, editable and enabled, and the same document layer/source/character selection must remain active. Composition locks and suspended character correspondence refuse edits. The existing graph parameter guards for locks, connections and expressions remain in force; a locked/driven Text Position does not lock its independent character overrides. Invalid layout/path/offset previews roll back to the previous valid preview. These checks apply before preview and before commit.

## Verification

- `tests/numeric-scrub.test.js`: threshold, units, finite inputs, mixed common-delta bounds and decimal/precision no-ops.
- `tests/character-numeric-scrub-dom.test.js`: actual rendered floating/Inspector fields, per-field and mixed edits, typography units, history/no-op/sparse record identity, pending draft, interruptions, readonly/disabled/detached protection, suspended correspondence, locked composition, leading and wrap width.
- `tests/character-numeric-scrub.e2e.js`: actual Chrome mouse hits on the input elements, both palettes, mixed typography, click/type/native arrows, interrupted gestures, pinned palette, zoom/modifiers and JSON/SVG persistence. Included in `npm run test:e2e`, with `test-results/workflow-character-numeric-scrub.png` as browser evidence when executed.
- Existing numeric-scrub, character-transform, rich-text, text-wrap and Shift-axis suites guard previous semantics.

Synthetic DOM results do not prove browser rendering. Record actual Chrome evidence for the exact candidate and distinguish local/CI execution and any route restrictions. Native WebView2 acceptance remains separate.
