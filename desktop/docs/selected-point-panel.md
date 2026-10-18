# Selection-first node point controls

2026-10-17. Bounded UI improvement, not the proposed full Inspector redesign.

Curve, owned Multi Path, and generated Path Remap node cards now start with a
point count and Canvas selection guidance instead of eagerly creating every
point's numeric controls. Canvas anchor selection/Shift-toggle is reflected by
stable point ID in the node panel. Selected rows have a blue edge; the active
point has a gold edge and its row is opened/revealed inside the scroll area.

At most 24 point rows are created per node card. Larger selections have pages.
Explicit **Browse all points** provides paged access to every point, including
ambiguous overlapping anchors that cannot be picked directly on Canvas. The
point button in a row selects that stable ID (Shift toggles it), returning to
selection-first display. A new Canvas selection also returns to this mode.

The existing field renderer and Session commands still own numeric values,
locks, sources, expressions, handle types, deletion and undo. No core/schema or
geometry conversion changes were made. Undo/document replacement resets point
selection and browsing state as part of the existing transient-state reset.

Remap's checkbox-based batch selection remains independent of Canvas point
selection and is explicitly labelled **batch checked**. Its delta fields appear
when at least one batch point is checked. Checkbox state survives point-panel
filtering and paging. Generated topology editing is not added; typed Remap
controls and retained-edit rebind remain available on displayed rows.

## Owned handle angle and length (REQ-31, 2026-10-19)

Curve and Multi Path point rows expose Incoming (handleA) and Outgoing (handleB)
Angle° / Length beside the existing X/Y controls, in the graph and Inspector.
Angles are document-local degrees clockwise from +X (+Y points down); lengths
are document units. Display uses three decimals; editing one component retains
the other component's full stored precision. Values remain relative Vector2
parameters in JSON: no schema change or additional saved angle state.

A zero-length handle displays 0° with its Angle input disabled. Increasing its
Length starts along +X; set a non-zero length before choosing another angle.
Negative lengths, empty and non-finite values are rejected. The shared safe
arithmetic input evaluator works with Enter; Escape discards the input draft.
These added fields support typing and native number steps, with no drag scrub.

Each accepted edit uses the same curve-point Session gesture as Canvas dragging,
including corner/smooth/symmetric coupling, locked/driven protection and one
History entry. An unchanged value preserves Undo/Redo. A zero-length edit follows
Canvas's existing coupling behavior (including collapsing the opposite handle
for linked modes); no NaN or remembered direction is introduced. Canvas edits,
X/Y edits, Undo/Redo and JSON reload recalculate the displayed polar values.
Focused point-panel edits retain focus and nested scroll across rendering.
Path Remap and multi-point batch editing are unchanged.

Focused checks: `node --test tests/handle-polar.test.js
tests/canvas-gesture-session.test.js`; real Chrome fixture:
`BASE_URL=http://127.0.0.1:5205 node tests/handle-polar.e2e.js` with the dev server
running on a disposable port. Set `CHROME_PATH` when necessary. The test creates
its own temporary browser profile and generated documents, then closes it.
It writes `test-results/handle-polar/receipt.json` and `inspector.png` locally.
Native Tauri/EXE and physical IME remain unverified.

## Verification boundary

Focused synthetic DOM tests cover empty/one/multiple selection, active ID,
Shift deselection, explicit paging, owned Text bake, generated Text to Path →
Remap, independent batch checks, numeric typing/scrub/undo, Alt handles and unit
moves. Existing core tests cover point-group commands, remapping and text bake.
The numeric-scrub DOM test's old hard-coded import rewrite did not resolve newer
relative imports; it now uses the same generic relative-import rewrite as the
other focused DOM tests.

This does not establish real-browser layout, Windows/EXE performance, or a
complete Inspector acceptance pass. No GUI/EXE test was run. Common multi-point
delta controls and virtualized rows remain separate work; this change bounds DOM
creation with paging instead.
