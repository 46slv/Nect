# Blank artwork entry points

**Launch** starts with no document, artboard, Layers or node graph. There is nothing to edit, save or export until you create a project or successfully open an existing one (including restoring a recovery copy).

**New** asks for the initial Width and Height in CSS px. Both fields start empty, with no preset. Enter two finite, positive values no greater than 100,000; fractional pixels are allowed. Invalid dimensions cannot create a project. After Create, the Save/Discard/Cancel guard protects any unsaved work. A successful New creates one blank Composition at the confirmed size with one vector Output. With a document open, **+ Blank** in Layers appends a minimal Layer without changing earlier artwork.

**Open** uses the dimensions stored in the project file without showing the size dialog. Cancelling New or Open, or opening an invalid file, does not install a document: an empty launch stays empty, and an existing project is preserved.

**Demo artwork** is separate from these entry points: neither launch nor Layers loads a demo. To see a demo, explicitly open a saved example such as [Boolean Remap](../examples/boolean-remap.json). The public `document()` and `layer()` factories still produce examples for callers and tests.

## Empty-output model

A blank composition opts in with the optional boolean `allowEmptyOutput: true`. Its designated terminal **Output** may evaluate to `{ contours: [] }` only when its PaintedPath input is literal null, has no source or source port, and has an empty expression. The saved schema is unchanged; existing projects without the flag keep their prior evaluation behavior.

The flag is validated strictly. It does not apply to Image Output, other Output nodes, Fill/Stroke, Boolean or arbitrary disconnected inputs. Missing Output/source IDs, wrong socket types, cycles and invalid expressions retain their diagnostics. Connecting a real source evaluates it normally; disconnecting the opted-in terminal returns to the intentional empty result. Disconnected graph nodes can be prepared while the terminal remains empty, then **Use as output** explicitly chooses the displayed route.

Empty geometry emits no path/group/paint element or selectable canvas hit. The blank canvas has no artwork box, source move marker, vertices or resize handles. Its Inspector says **Empty composition** and explains the next actions. Selecting the empty layer row does not pretend it is visible artwork. Graph Output selection remains available for explicit graph editing.

## History and existing work

- Cancelling the size dialog leaves the current project, its file binding and history unchanged. New uses the existing unsaved Save/Discard/Cancel guard after dimensions are confirmed; cancelling that guard also leaves the current document and history unchanged. An active edit or Pen draft must be finished or cancelled before changing documents. If the project or active Composition changes while the size dialog or the unsaved decision is pending, the old request cannot replace the newer work.
- Confirming New replaces the project with independent, empty Undo/Redo history. Undo cannot recover the replaced project; save existing work before discarding it.
- Closing with no document does not prompt to save. A newly created project is unsaved until **Save JSON** and is covered by the unsaved-close warning.
- **+ Blank** first cancels any pressed edit/wire gesture before changing layer scope. Rejected append likewise retains the pre-gesture committed document. It is one Undo unit, works from hidden/locked/image contexts and preserves prior nodes, wires, Output, visibility, locks and assets. Candidate validation and the 100-layer limit run before append. Capacity or invalid-document rejection preserves history and selection.
- Circle/Rectangle/Polygon/Star/Text and Pen continue to create independent artwork from the blank scene. Undo removes only that creation and restores the exact empty selection. The initial blank layer remains available for explicit graph work.

## Persistence and verification

Save/reload retains the confirmed dimensions and intentional empty Composition. Empty SVG is the artboard element without content; PNG has transparent pixels at the selected export scale. Adding subsequent artwork and exporting the mixed document omits the empty layer without omitting valid artwork.

`tests/blank-artwork.test.js` covers factory behavior, strict opt-in/cache behavior, reachable malformed graphs, empty export, candidate preservation, IDs, capacity and history. `tests/blank-artwork-dom.test.js` covers launch without a document, New size validation and stale decisions, existing-document opening, New/+Blank, dirty state, draft cancellation, selection, toolbar entry points and explicit graph routing. `tests/blank-artwork.e2e.js` covers the browser New/Save/Open flow and confirmed dimensions, open/pressed Pen cancellation, exact Undo/Redo, transparent SVG/PNG, independent creation, +Blank capacity and layout. Native WebView2, native file dialogs and physical Windows IME remain separate acceptance gates.

[Editable blank document](../examples/blank-document.json).
