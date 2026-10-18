# Graph Copy / Paste

Select graph nodes and press **Ctrl+C / Ctrl+V** (Cmd on macOS). The header
context menu also has Copy/Paste. Focus must be in the graph. Text fields,
ordinary selected text, dialogs, IME and other viewports retain native shortcuts.

The clipboard is **inside this open Document only**. It is not the OS clipboard,
does not export image/font data, and resets on New/Open/reload. Saved pasted
nodes remain ordinary authored nodes and reopen normally.

Copy captures the authored settings at that moment. Paste creates fresh node,
point and contour IDs using the existing duplication contract, remaps internal
connections/expressions/references, preserves relative graph placement, selects
the pasted nodes and records one Undo. Each subsequent Paste offsets the group.
Output nodes are skipped with a visible count; the destination retains its own
Output. Use as output connects the chosen pasted result explicitly.

In the original Composition, external inputs keep referencing the original
nodes, with a visible notice. Pasting into another Composition requires those
input/reference sources to be included in Copy. Otherwise Paste refuses and
asks to copy the sources too. It never reconnects to a similar name or ID.

Embedded Image/font assets and SVG/Composition Reference targets stay shared
by their exact same-document identities. Missing/changed assets, missing source
Compositions, locked destinations and cycles refuse atomically. An edited Path
Remap together with its source retains the existing duplication refusal; no
retained point edits are silently rebound. Node/resource/document limits remain.

This checkpoint includes the earlier explicit safe DejaVu replacement fix.
Text canvas-drag recovery and recovery autosave are next, separate requests.
