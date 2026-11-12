# Layer Folders

A Folder organizes Layers in one outer Composition. Create an empty Folder, or
select adjacent root Layers and create a Folder around them. Select adjacent
root Layers and use **Add** to add them to a Folder. Select Layers at either
edge and use **Move out** to return them to the root. **Ungroup** removes the
Folder and leaves every Layer in place. Double-click the name or use F2 to
rename it. The disclosure control expands or collapses its rows.

Folder IDs, names and exact member Layer IDs are authored per Composition and
survive Undo/Redo, Project Import and Save/Open. Empty Folders appear first in
the panel, newest first. A populated Folder appears at its contiguous Layer
span. Collapse is a per-Composition window view state: it does not hide artwork,
change the active Layer, add History or dirty the project.

The existing Layer array remains the only paint order. Organizing, renaming,
collapsing and ungrouping never change Layer records, references or output.
Moving across a Folder boundary through ordinary reorder is refused; move into
or out of the Folder explicitly. Duplicate stays in its source Folder; deleting
its last member keeps the empty Folder. SVG replacement replaces the active
Composition and its Folder organization in the same Undo step.

This first slice supports one Folder level and only moves that preserve the
current paint order. Nested Folders, positioning an empty Folder at an arbitrary
boundary, and reparenting that changes paint order remain unfinished.
