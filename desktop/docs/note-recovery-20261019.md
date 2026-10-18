# SS-Note-ROG source recovery and integration

## Source preservation

The two pre-existing worktrees were inspected before mutation. Existing Notion,
ChatGPT, Terminal and other processes were left running. No Nect development
process or open Chrome tab was present in the initial inventories. This does not
prove other applications had no unsaved content; none was closed or overwritten.

Original HEAD bundles, binary working/index patches, every modified/untracked
source file and SHA256 manifests are retained outside this repository at
`../recovery-20261019/{context-menu,opentype}` in the task workspace.

| Original branch | Recovered remote HEAD | Verification before push |
| --- | --- | --- |
| `codex/context-menu-native-20261018` | `ff301f56d84c7bc68bf2430031283b4e8a84e7d2` | 18 focused tests; Vite build |
| `codex/opentype-controls-20261018` | `d9388421ea30a2af3f928b2ded5063c49c571a14` | 10 focused tests; Vite build; includes `ef16eb4` |

Both ordinary pushes were checked against `git ls-remote`. OpenType's two initial
test failures were corrected against the existing contracts: use a controlled
kerning fixture and the public `glyphAdvances` projection. No production change
was required for those failures.

## Integration boundary

Independent branch `codex/note-recovery-integration-20261019` starts at
`2f57db3ed469080c272e43b67546b1360ce26353`, the shared Draft PR27 source.
The original worktrees remain separate. Nothing was merged to main or PR27.
The new-PC local `f54505a` was neither fetched nor imported as a dependency.

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

Real UI used the existing SS-Note-ROG Chrome extension connection and an owned
tab at `http://127.0.0.1:5199/`. All content was generated disposable fixture
data. The sandbox-started server timed out from Chrome; the same Vite command
was restarted through the formal elevated host-execution route. No browser,
authentication, network or security setting was changed.

| Actual Chrome operation | Observed result |
| --- | --- |
| Circle radius `120/3` | `40` |
| Graph context Duplicate / Delete + Undo | node counts `3→4→3`, `3→2→3` |
| Layer context Duplicate / Delete + Undo | layer counts `2→3→2`, `2→1→2` |
| Canvas context Duplicate / Delete + Undo | layer counts `2→3→2`, `2→1→2` |
| Reopen menu after Undo/rerender | custom menu remains open after the repair |
| Layer Backward + Undo | order changes and returns exactly |
| Layer row Ctrl+D + Undo | one copy, one Undo restores original count |
| Locked layer context menu | Duplicate/Delete disabled with explicit reason |
| Layer-name input Ctrl+A and typing | exact replacement text retained |
| Project composition file chooser / Import / Undo | generated fixture listed, counts `2→3→2` |
| Latin `office ffi`, liga On/Off/Undo | path-data lengths `4085→4049→4085`; both Undo states match exact prior paths |
| Graph pan pad right | scroll X `40→120`, Y stays `73` |

Chrome warn/error log query returned an empty list. Real right-click actions
opened the app menus; the DOM tests additionally verify native-menu default
prevention on Layers, portals, dialogs, margins and graph/canvas, while text
fields retain their ordinary context-menu event. Native browser chrome menus
were not independently inspected through a native-app capture.

## Explicit limitations and approval state

- Actual Layer Ctrl+C/V remains **unverified**: Browser Use's virtual clipboard
  handled those chords before the app shortcut. The documented raw-CDP Input
  method returned unsupported and was not retried. Real Ctrl+D and synthetic
  DOM Layer Ctrl+C/V tests passed. This is not recorded as a product failure.
- Layer drag reorder was not exercised; the real reorder button was. Native
  app/WebView2, physical IME, native file dialogs, restart and distribution were
  not run in this recovery task.
- Parent identified approval ID `manager:4854b3a392d24efb99d2bdc69f282813:1`
  as localhost fixture-file loading and forwarded explicit human authorization.
  The original formal file-selection tool already returned completion and the
  correct composition list was visible; import and Undo then succeeded. No
  retry was necessary. There is no known outstanding approval-dependent action.
- A parent progress-message attempt through the app-native thread tool returned
  `thread not found`; it did not deliver. The final delegated response is the
  reliable return route. No account, permission or authentication change is
  needed for the remaining authorized source work.

Local review followed the shared coding review/debug/Git owners. External
consultation gate was invoked and recorded **not-required**, zero submissions:
local source, Session/DOM tests and actual UI resolved the material decisions.
