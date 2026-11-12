# Blank artwork entry points

**New** asks for the initial Width and Height in document px before creating an empty project with one blank Composition and one vector Output. The initial fields show 960 × 540; both values must be finite, positive and at most 100,000. After Create, the existing Save/Discard/Cancel guard protects the current project. **+ Blank** in Layers appends a minimal Layer without changing earlier artwork. The editor starts with the existing 960 × 540 blank document and no selected node or artwork.

Production startup and Layers have no demo entry point. The public `document()` and `layer()` factories retain their explicit example behavior for existing callers and fixtures; saved documents are unchanged. Startup reuses `blankDocument()` rather than a separate scene definition. Open, Save and unsaved-change guards retain their existing behavior.

## Empty-output model

A blank composition opts in with the optional boolean `allowEmptyOutput: true`. Its designated terminal **Output** may evaluate to `{ contours: [] }` only when its PaintedPath input is literal null, has no source or source port, and has an empty expression. The saved schema is unchanged by blank startup; existing projects without the flag keep their prior evaluation behavior.

The flag is validated strictly. It does not apply to Image Output, other Output nodes, Fill/Stroke, Boolean or arbitrary disconnected inputs. Missing Output/source IDs, wrong socket types, cycles and invalid expressions retain their diagnostics. Connecting a real source evaluates it normally; disconnecting the opted-in terminal returns to the intentional empty result. Disconnected graph nodes can be prepared while the terminal remains empty, then **Use as output** explicitly chooses the displayed route.

Empty geometry emits no path/group/paint element or selectable canvas hit. The blank canvas has no artwork box, source move marker, vertices or resize handles. Its Inspector says **Empty composition** and explains the next actions. Selecting the empty layer row does not pretend it is visible artwork. Graph Output selection remains available for explicit graph editing.

## History and existing work

- Cancelling the size dialog leaves the current project, its file binding and history unchanged. New uses the existing unsaved Save/Discard/Cancel guard after dimensions are confirmed; cancelling that guard also leaves the current document and history unchanged. An active edit or Pen draft must be finished or cancelled before changing documents. If the project or active Composition changes while the size dialog or the unsaved decision is pending, the old request cannot replace the newer work.
- Confirming New replaces the project with independent, empty Undo/Redo history. Undo cannot recover the replaced project; save existing work before discarding it.
- Launch and New are unsaved until **Save JSON**. The existing unsaved-close warning remains active.
- **+ Blank** first cancels any pressed edit/wire gesture before changing layer scope. Rejected append likewise retains the pre-gesture committed document. It is one Undo unit, works from hidden/locked/image contexts and preserves prior nodes, wires, Output, visibility, locks and assets. Candidate validation and the 100-layer limit run before append. Capacity or invalid-document rejection preserves history and selection.
- Circle/Rectangle/Polygon/Star/Text and Pen continue to create independent artwork from the blank scene. Undo removes only that creation and restores the exact empty selection. The initial blank layer remains available for explicit graph work.

## Persistence and verification

Save/reload retains the confirmed dimensions and intentional empty Composition. Empty SVG is the artboard element without content; PNG has transparent pixels at the selected export scale. Adding subsequent artwork and exporting the mixed document omits the empty layer without omitting valid artwork.

`tests/blank-artwork.test.js` covers factory behavior, strict opt-in/cache behavior, reachable malformed graphs, empty export, candidate preservation, IDs, capacity and history. `tests/blank-artwork-dom.test.js` covers blank launch, New size validation and stale decisions, existing-document opening, New/+Blank, dirty state, draft cancellation, selection, toolbar entry points and explicit graph routing. `tests/blank-artwork.e2e.js` covers the browser New/Save/Open flow and confirmed dimensions, open/pressed Pen cancellation, exact Undo/Redo, transparent SVG/PNG, independent creation, +Blank capacity and layout. Native WebView2, native file dialogs and physical Windows IME remain separate acceptance gates.

[Editable blank document](../examples/blank-document.json).
