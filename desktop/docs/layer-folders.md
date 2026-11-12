# Layer Folders

A Folder organizes Layers in one outer Composition. Create an empty Folder, or
select adjacent root Layers and create a Folder around them. Select Layers in
the Composition and use **Move here** to move them into a Folder. Select Layers at either
edge and use **Move out** to return them to the root. **Ungroup** removes the
Folder and leaves every Layer in place. Double-click the name or use F2 to
rename it. The disclosure control expands or collapses its rows.

Folder IDs, names and exact member Layer IDs are authored per Composition and
survive Undo/Redo, Project Import and Save/Open. Empty Folders appear first in
the panel, newest first. A populated Folder appears at its contiguous Layer
span. Collapse is a per-Composition window view state: it does not hide artwork,
change the active Layer, add History or dirty the project.

The existing Layer array remains the only paint order. When membership alone
keeps every Folder contiguous, **Move here** leaves that order unchanged.
Otherwise it shows the current and proposed orders before moving the selected
Layers as a block to the front of the destination. The relative order of the
selected Layers, and of the remaining Layers, is preserved. An empty destination
uses the front of the Composition. Cancellation leaves the project and History
unchanged. Renaming, collapsing and ungrouping do not change the paint order.
Moving across a Folder boundary through ordinary reorder is refused; move into
or out of the Folder explicitly. Duplicate stays in its source Folder; deleting
its last member keeps the empty Folder. SVG replacement replaces the active
Composition and its Folder organization in the same Undo step.

Folders currently have one level. Nested Folders, positioning an empty Folder at
an arbitrary boundary, and choosing another insertion point remain unfinished.
