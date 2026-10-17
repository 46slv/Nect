# Blank artwork entry points

**New** starts an empty 960 × 540 document with one blank composition and one vector Output. It asks before replacing the current project. **+ Blank** in Layers appends the same minimal composition without changing earlier artwork. The editor starts with this same blank document and no selected node or artwork.

Production startup and Layers have no demo entry point. The public `document()` and `layer()` factories retain their explicit example behavior for existing callers and fixtures; saved documents are unchanged. Startup reuses `blankDocument()` rather than a separate scene definition. Open, Save and unsaved-change guards retain their existing behavior.

## Empty-output model

A blank composition opts in with the optional boolean `allowEmptyOutput: true`. Its designated terminal **Output** may evaluate to `{ contours: [] }` only when its PaintedPath input is literal null, has no source or source port, and has an empty expression. The saved schema is unchanged by blank startup; existing projects without the flag keep their prior evaluation behavior.

The flag is validated strictly. It does not apply to Image Output, other Output nodes, Fill/Stroke, Boolean or arbitrary disconnected inputs. Missing Output/source IDs, wrong socket types, cycles and invalid expressions retain their diagnostics. Connecting a real source evaluates it normally; disconnecting the opted-in terminal returns to the intentional empty result. Disconnected graph nodes can be prepared while the terminal remains empty, then **Use as output** explicitly chooses the displayed route.

Empty geometry emits no path/group/paint element or selectable canvas hit. The blank canvas has no artwork box, source move marker, vertices or resize handles. Its Inspector says **Empty composition** and explains the next actions. Selecting the empty layer row does not pretend it is visible artwork. Graph Output selection remains available for explicit graph editing.

## History and existing work

- New uses the existing unsaved Save/Discard/Cancel guard. Cancelling leaves the current document and history unchanged; an active edit or Pen draft must be finished or cancelled before changing documents.
- Confirming New replaces the project with independent, empty Undo/Redo history. Undo cannot recover the replaced project; save existing work before discarding it.
- Launch and New are unsaved until **Save JSON**. The existing unsaved-close warning remains active.
- **+ Blank** first cancels any pressed edit/wire gesture before changing layer scope. Rejected append likewise retains the pre-gesture committed document. It is one Undo unit, works from hidden/locked/image contexts and preserves prior nodes, wires, Output, visibility, locks and assets. Candidate validation and the 100-layer limit run before append. Capacity or invalid-document rejection preserves history and selection.
- Circle/Rectangle/Polygon/Star/Text and Pen continue to create independent artwork from the blank scene. Undo removes only that creation and restores the exact empty selection. The initial blank layer remains available for explicit graph work.

## Persistence and verification

Save/reload retains the intentional empty composition. Empty SVG is the artboard element without content; PNG has transparent pixels at the selected export scale. Adding subsequent artwork and exporting the mixed document omits the empty layer without omitting valid artwork.

`tests/blank-artwork.test.js` covers factory compatibility, strict opt-in/cache behavior, reachable malformed graphs, empty export, candidate preservation, IDs, capacity and history. `tests/blank-artwork-dom.test.js` covers blank launch, absence of demo UI, existing-document opening, New/+Blank, dirty state, draft cancellation, selection, all toolbar entry points and explicit graph routing. `tests/blank-artwork.e2e.js` is the separate Chrome harness for New confirmation, open/pressed Pen cancellation, exact Undo/Redo, transparent SVG/PNG, cold JSON reopen, independent creation, +Blank capacity and 1280×800 / 1600×1000 layouts. The startup change was checked with synthetic DOM tests and a production Web build; this Chrome harness was not run for this change. Native WebView2, file dialogs and Windows IME remain separate acceptance gates.

[Editable blank document](../examples/blank-document.json).
