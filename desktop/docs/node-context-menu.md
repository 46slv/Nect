# Node header context actions

Implements the October 17 request to put **Use as output**, **Duplicate** and
**Delete** in the node-name/header right-click menu. Existing buttons and shortcuts
remain available. The menu also works on collapsed headers.

- Right-clicking a selected node preserves the selection set and makes that node
  active. Right-clicking an unselected node selects only that node. This is UI state
  and creates neither an authored change nor an Undo entry.
- Use as output applies only to the clicked node. It uses the existing
  `preview-node` command, including its Fill insertion for unpainted paths. It
  rewires the composition Output; it is distinct from the independent Viewers.
- Duplicate uses the same helper and `duplicate-nodes` command as the existing
  button. A selected group is copied as one transaction with internal references
  remapped. The shared helper now places both single nodes and groups beside the
  source selection; see [adjacent duplication](node-duplication.md).
- Delete uses `delete-nodes` for the selection. Output and Image Output nodes,
  including the composition's designated output, are retained. References to
  deleted nodes are disconnected by the common Session command. One Undo restores
  the group.
- Locked compositions disable all actions. Unsupported output types, incompatible
  composition outputs and locked Output inputs have disabled items with reasons.
  Existing duplication limits and the edited Path Remap/source restriction are
  shown before execution. Core validation remains authoritative for cycles and
  other semantic errors; failures do not partially commit.
- Finish/cancel active gestures, rename, IME, modal edits or asynchronous variable
  font preparation before opening the menu. Text inputs retain their native
  context menu. No input socket disconnection behavior is changed.
- Escape, Tab, an outside pointer press, scrolling, resizing, another render or
  document replacement dismisses the menu. Arrow keys and Home/End navigate enabled
  items; Enter/Space activates one. Dismissed/stale menu elements cannot execute.

## Verification

`node --test tests/node-context-menu-dom.test.js tests/multi-selection-feedback-dom.test.js tests/command-session.test.js`
passes 30 tests (including parent test containers). This covers real application
handlers in synthetic DOM, not actual browser rendering or native WebView input.
The existing `editor-names-bypass-dom.test.js` additionally ran and failed at its
unrelated schema-version assertion (line 167 expects 10, document is 24). This
change does not modify the schema, core or that older test.

Production Web build passed (97 modules); targeted formatting checks passed.
No EXE packaging, browser screenshot or native interaction acceptance is claimed.
