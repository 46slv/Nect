# Graph Copy / Paste

Select graph nodes and press **Ctrl+C / Ctrl+V** (Cmd on macOS). The header
context menu also has Copy/Paste. Focus must be in the graph. Text fields,
ordinary selected text, dialogs, IME and other viewports retain native shortcuts.

The clipboard is **inside this open Document only**. It is not the OS clipboard,
does not export image/font data, and resets on New/Open/reload. Saved pasted
nodes remain ordinary authored nodes and reopen normally. A self-contained Copy
and its subsequent Paste offset survive outer Composition tab switches, ordinary
Composition create/rename/resize/delete, and project Undo/Redo. Copy and tab
switches do not add History or dirty the project. Existing SVG/project Import
paths still reset Copy.

Only self-contained node subgraphs cross outer Composition tabs. A Copy with
unselected input/reference nodes, numeric/Color Artwork references, or a legacy
Layer-graph Composition Reference is cleared on that crossing. Include local
input sources in Copy to make a closed subgraph; foreign Artwork and Layer-graph
references remain local. This never reconnects to coincident destination IDs.
A restored foreign Artwork source does not revive a reference which expired
while it was absent; Copy again.

Copy captures the authored settings at that moment. Paste creates fresh node,
point and contour IDs using the existing duplication contract, remaps internal
connections/expressions/references, preserves relative graph placement, selects
the pasted nodes and records one Undo. Each subsequent Paste offsets the group.
Output nodes are skipped with a visible count; the destination retains its own
Output. Use as output connects the chosen pasted result explicitly.

In the original Artwork (Layer graph), external inputs keep referencing the
original nodes, with a visible notice. Pasting into another Artwork requires those
input/reference sources to be included in Copy. Otherwise Paste refuses and
asks to copy the sources too. It never reconnects to a similar name or ID.

An expression that references a numeric or Color property in another Artwork can
be pasted within its original Artwork, retaining its source. Color references
require a destination already at schema 64 or later and do not create shared
Color bindings. If that source layer, node or Curve point is
missing or replaced after Copy, restore it and Copy again; the same ID appearing
later does not silently revive the clipboard reference. Pasting these nodes into
another Artwork and copying references to another outer Composition are currently
refused.

Embedded Image/font assets stay shared by their exact same-project identities.
Legacy SVG/Composition Reference targets stay shared within their original outer
Composition; they do not render or transfer another outer Composition. Missing/changed assets, missing source
Compositions, locked destinations and cycles refuse atomically. An edited Path
Remap together with its source retains the existing duplication refusal; no
retained point edits are silently rebound. Node/resource/document limits remain.

This checkpoint includes the earlier explicit safe DejaVu replacement fix.
Text canvas-drag recovery and recovery autosave are next, separate requests.
