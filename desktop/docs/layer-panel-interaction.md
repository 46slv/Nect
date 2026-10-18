# Layer panel interaction handoff

## Scope implemented

- Clicking a Layer row keeps the existing stable-ID selection behavior and moves keyboard focus to that row.
- Dragging a row onto another row commits one `reorder-layer` history command. IDs, graph references, locks, and assets remain unchanged.
- `Ctrl/Cmd+C` stores selected layer IDs in an in-document clipboard. It never writes to the OS clipboard.
- `Ctrl/Cmd+V` duplicates the stored layer through the existing whole-artwork duplication path, including fresh IDs and one Undo.
- `Ctrl/Cmd+D` duplicates the focused layer through the same path.
- Text inputs, textareas, selects, dialogs, IME composition, node shortcuts, and graph focus retain their existing behavior.

## Existing behavior intentionally reused

- `order-layer`, `duplicate-artworks`, layer selection, locked-layer checks, and history are existing core/UI paths.
- Existing row actions for visibility, Solo, guide, lock, delete, rename, and Forward/Backward remain unchanged.
- Common UI rule: the same command may have multiple entrances (row button, shortcut, or context menu). Convenience buttons are not removed for deduplication; every entrance uses the same result, Undo unit, and enable/lock conditions.

## Deferred integration

- Native right-click menu integration remains owned by the parent task. The old `nect-context-menu` branch contains uncommitted context-menu changes and must be recovered before merging that surface.
- This branch does not reimplement `main.js` document context prevention, Graph Layer Canvas Duplicate/Delete, `core.js` atomic delete, or `artwork-deletion.js` work from that branch.

## Verification receipt

- Base: `5f8f3a5e2b2e2867f6ed49a4a0a8c80f1fa279da`
- `npm run build`: passed.
- Focused `serialize` → `deserialize` roundtrip: passed for duplicated blank Compositions; layer order and unique remapped IDs survived.
- `npm test -- --runInBand`: executed; the repository baseline currently reports unrelated failures across existing artwork/UI/e2e fixtures. No build or module parse error was introduced by this change.
