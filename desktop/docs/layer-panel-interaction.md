# Layer panel interaction handoff

## Scope implemented

- Clicking a Layer row keeps the existing stable-ID selection behavior and moves keyboard focus to that row.
- Drag the Layer name or row padding to the line above/below another row. A move commits one `reorder-layer` history command. The list remains front-to-back; authored records and SVG painting remain back-to-front. IDs, graph references, locks, and assets remain unchanged.
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

## Layer reorder repair / 2026-10-20

The delivered corrected aca9f590 trial could not start a drag from a Layer name because the name is a button and the row handler excluded all buttons. Dropping onto a neighboring row also used the wrong insertion side for the reversed list. A focused synthetic DOM reproduction at acca2616 failed on both paths before repair.

Names and row padding now arm a pointer reorder. Crossing six CSS pixels starts capture and a visible insertion line. Capture is delayed so an ordinary name click/double-click retains selection/rename behavior. Release maps the displayed upper/lower half to the corresponding authored array slot, including its frontmost boundary. The common reorder-layer command uses beforeLayerId: null for that frontmost boundary; existing ID targets retain their prior semantics. No saved format changes.

The drag binds its pointer, Session/revision and document. Escape, blur, pointer cancellation, capture loss, a changed document/revision, and busy editing discard it. Locked sources refuse; other row buttons remain actions. A drag's compatibility click is consumed, so it cannot select/rename a different row after committing. No-op drops preserve Redo.

Validation: 18/18 PASS across tests/layer-reorder-dom.test.js, tests/artwork-ordering.test.js and tests/artwork-ordering-dom.test.js. Assertions include exact record permutation, unchanged selection, click/cancellation/stale/lock guards, existing Forward/Backward, one Undo/Redo, JSON codec and SVG order. These are synthetic DOM/core checks, not native WebView2 acceptance. Native build is unavailable here: cargo/rustup/rustc absent, Windows SDK26100 absent and MSVC unresolved. No server/browser/native launch or external UI focus was used.

Targeted native retest after a distinct candidate is built: use a copy with three overlapping, differently colored Layers; drag the middle Layer name above its neighbor and then below, including the top and bottom insertion edges. The insertion line, list order and overlapping color must agree. One Undo and one Redo must restore each complete order. Confirm ordinary name click/rename, existing Forward/Backward and locked-source refusal; Save As, restart and export SVG/PNG must retain the chosen order. Record the exact executable/source/hash; do not mark checklist03 native PASS from these source tests.
