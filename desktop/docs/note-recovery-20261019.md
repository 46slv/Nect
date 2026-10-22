# Context-menu and OpenType integration

## Integration boundary

This checkpoint combines context-menu actions and OpenType controls while
retaining the shared Session's arithmetic, artwork and graph behavior. The
initial source checks passed 18 context-menu tests and 10 OpenType tests, plus
production builds. Two OpenType test repairs used a controlled kerning fixture
and the public `glyphAdvances` projection; they did not require product changes.

The `core.js` import conflict retains both `moveArtworkLayerBefore` and
`removeArtworkLayers`. The main/core OpenType hunks retain numeric arithmetic,
layer reorder/generic duplication, composition import, command menus and pan.
OpenType adds schema 38; prior accepted versions and Repeater schema 37 remain.
Import/duplicate/delete/Undo tests explicitly retain the promoted schema.

Required integration repairs:

- Single-layer context duplication uses the latest shared `duplicate-layer`
  command, including intentionally empty layers. Multi-artwork duplication keeps
  its existing bounded eligibility and atomic Session command.
- Menus ignore queued scroll notifications when the owner is still at its
  opening position. Actual graph/panel/document scroll still dismisses them.
  This fixes the Chrome-reproduced disappearance after Undo and rerender.
- The DOM harness supplies `MutationObserver`, now needed by command menus.
  The duplication fixture preserves the new document's schema floor instead of
  assigning schema 10 to a document with a schema-27 centered grid.

## Verification

Final focused run: **75/75**. Additional command/artwork/text/readonly/gesture
Session regression run: **24/24**. Final Vite production build passes, 124 modules.
Logs are in ignored `desktop/test-results/note-recovery/`:
`focused-final.log`, `session-regression.log`, `build.log`.
This is not a full-suite or native-build claim.

Real Chrome UI checks used generated disposable fixture data.

| Actual Chrome operation                          | Observed result                                                              |
| ------------------------------------------------ | ---------------------------------------------------------------------------- |
| Circle radius `120/3`                            | `40`                                                                         |
| Graph context Duplicate / Delete + Undo          | node counts `3→4→3`, `3→2→3`                                                 |
| Layer context Duplicate / Delete + Undo          | layer counts `2→3→2`, `2→1→2`                                                |
| Canvas context Duplicate / Delete + Undo         | layer counts `2→3→2`, `2→1→2`                                                |
| Reopen menu after Undo/rerender                  | custom menu remains open after the repair                                    |
| Layer Backward + Undo                            | order changes and returns exactly                                            |
| Layer row Ctrl+D + Undo                          | one copy, one Undo restores original count                                   |
| Locked layer context menu                        | Duplicate/Delete disabled with explicit reason                               |
| Layer-name input Ctrl+A and typing               | exact replacement text retained                                              |
| Project composition file chooser / Import / Undo | generated fixture listed, counts `2→3→2`                                     |
| Latin `office ffi`, liga On/Off/Undo             | path-data lengths `4085→4049→4085`; both Undo states match exact prior paths |
| Graph pan pad right                              | scroll X `40→120`, Y stays `73`                                              |

Chrome warn/error log query returned an empty list. Real right-click actions
opened the app menus; the DOM tests additionally verify native-menu default
prevention on Layers, portals, dialogs, margins and graph/canvas, while text
fields retain their ordinary context-menu event. Native browser chrome menus
were not independently inspected through a native-app capture.

## Verification limitations

- Actual Layer Ctrl+C/V remains **unverified**: Browser Use's virtual clipboard
  handled those chords before the app shortcut. The documented raw-CDP Input
  method returned unsupported and was not retried. Real Ctrl+D and synthetic
  DOM Layer Ctrl+C/V tests passed. This is not recorded as a product failure.
- Layer drag reorder was not exercised; the real reorder button was. Native
  app/WebView2, physical IME, native file dialogs, restart and distribution were
  not run in this recovery task.
