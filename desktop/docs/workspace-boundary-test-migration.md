# Workspace boundary test migration

The reviewed layout makes drawers mutually exclusive and opens the Inspector and
boundary controls only on request. Tests that previously clicked hidden controls
must use the visible trigger; changing layout is not permission to bypass inert.

- **Keep:** editing, document and project integrity, import/export, undo/redo,
  selection, focus/IME, pointer cancellation, retained Assets owner and camera
  invariants. These protect product behavior rather than a particular layout.
- **Update:** fixed sidebar coordinates, both-drawers-open expectations and the
  old width-dependent Graph More disclosure. Assert containment, exclusivity,
  reachable commands and stable cameras against the new boundary instead.
- **Consolidate:** repeated static width/height checks into representative wide,
  narrow and short-split browser scenarios; retain focused interruption tests.
  The startup inventory now checks setup ordering instead of manual counts and
  literal neighboring lines; menu-observer coverage checks real delivery and
  disconnect rather than duplicating the full menu topology.
- **Remove:** no editing or persistence coverage. Delete an assertion only when
  its old visual arrangement is impossible by the new contract and replace it
  with an assertion of the corresponding new behavior.

CI runs and screenshots must be attributed to their exact commit. A formatter
receipt or an isolated unit pass is not a substitute for the final aggregate run.
