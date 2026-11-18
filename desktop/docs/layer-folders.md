# Layer Folders

A Folder organizes Layers in one outer Composition. Create an empty Folder, or
select adjacent root Layers and create a Folder around them. Select Layers in
the Composition and use **Move here** to move them into a Folder. Select Folder
Layers and use **Move out** to return them to the root. **Ungroup** removes the
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
unchanged. **Move out** also keeps the order when the remaining Folder members
are contiguous; otherwise it asks before moving the selection immediately in
front of the remaining Folder. Renaming, collapsing and ungrouping do not change the paint order.
The Folder's forward/backward controls move its populated span over the adjacent
root Layer or populated Folder. The current and proposed paint orders are shown
before the move; Cancel leaves the Project and History unchanged. A Folder with
locked members cannot move, and an empty Folder has no paint-order position.
Moving across a Folder boundary through ordinary reorder is refused; move into
or out of the Folder explicitly. Duplicate stays in its source Folder; deleting
its last member keeps the empty Folder. SVG replacement replaces the active
Composition and its Folder organization in the same Undo step.

Folders currently have one level. Nested Folders, positioning an empty Folder at
an arbitrary boundary, and choosing another insertion point remain unfinished.

## Neutral nested Folders: bounded design (not implemented)

REQ-52's Confirmed/Must requirement is preservation when organizing adjacent
siblings into a neutral Group. This stage adds hierarchical organization, not
rendering Groups, transforms, effects, cross-Composition reparenting or implicit
paint-order changes. The existing Layer array remains the only paint order.

### Authored representation and validation

Use an optional `parentId` on each Folder; omission means root. Keep `members`
as direct `{layerId}` entries, never duplicate descendant membership in ancestors.
A Layer belongs to at most one direct Folder. Each Folder has at most one parent,
which must exist in the same Composition and differ from itself. Reject cycles,
unknown parents, duplicate IDs/members, and foreign Layer references atomically.
The existing 100-Folder limit bounds traversal depth and work.

The recursively collected descendant Layers of every populated Folder must form
one contiguous interval in the unchanged Layer array. Direct members need not be
adjacent to each other when child Folder intervals occupy the intervening slots.
Sibling Folder intervals must not overlap or interleave. Empty Folders have no
paint interval; order empty siblings using the existing newest-first convention,
within their parent. Do not invent a paint position for an empty Folder.

### One practical grouping operation

Extend the existing create-Folder command with explicit selected `folderIds`
alongside `layerIds`. All selections must be direct siblings under one parent;
selecting both an ancestor and its descendant is invalid. The union of selected
nonempty descendant spans must be contiguous and contain no unselected sibling.
Create the new Folder at that same parent, move selected direct Layer memberships
into it, and set the selected Folders' parentId to it. No Layer array, node,
geometry, paint, output wiring, active Layer or transform changes. One Session
History entry; a failure commits nothing. Reject grouping if any selected Layer
or descendant Layer is locked, or the owning Composition is locked.

UI must allow selecting sibling Folder headers together with sibling Layers,
then creating a containing Folder. It must render nested indentation and preserve
per-Composition collapse state without hiding/rendering changes or History.
Collapsed ancestors hide descendant rows only. Selection of an ancestor and its
contents must not accidentally duplicate the operation. Empty selections retain
the existing root empty-Folder behavior; an all-empty sibling selection is valid.

### Ungroup, deletion and existing moves

Ungroup removes only that Folder: promote its direct Layer memberships into its
parent (or root), and reparent its immediate child Folders to the same parent.
Keep descendant contents, exact Layer order and child identities. Reject if any
affected descendant Layer or Composition is locked. Deleting the last Layer
removes its direct membership only and retains empty ancestor/child Folders.
Duplicating a Layer keeps its direct Folder and must leave every ancestor interval
contiguous. Folder rename/collapse remains metadata/view-only respectively;
this stage adds no destructive recursive Folder-delete action.

Existing flat Move here/Move out/forward/backward behavior remains unchanged for
unrelated flat branches. For this first nested stage, these existing move actions
must refuse an affected nested branch before mutation (and be disabled/explained
in its UI), rather than applying old direct-member algorithms to descendants.
Users can Ungroup and regroup adjacent siblings to reorganize a nested branch.
Ordinary Layer reorder inside a direct Folder may proceed only when the complete
candidate validates all ancestor intervals; crossing any nested boundary rejects.
Any otherwise-flat move crossing a nested subtree must also reject. Do not allow
an unselected nested branch to lose a member through a legacy move.

### Readback, import and acceptance

Absent parentId preserves old flat documents exactly; do not materialize nulls or
flatten nested data on read. Extend the shared Folder validator and all formal
Composition/legacy-document read paths. Project import/ID collision remapping must
remap parentId together with Folder IDs and direct Layer IDs; missing references
reject instead of detaching. Save/read/Undo/Redo preserve hierarchy and stable IDs.
No new rendering format or scheduler is introduced. An older build rejecting the
new field is preferable to silently discarding the hierarchy.

Focused proof must include Folder+Layer wrapping, nested wrapping, unchanged SVG
and Layer order, empty siblings, locked descendants, ancestor selection, cycles,
noncontiguous unions, unknown/cross-Composition IDs, Ungroup promotion, last-Layer
delete, duplicate/reorder, refused legacy moves including a nested neighbor,
flat compatibility, import collisions, Session History and DOM selection/collapse.
Native-window interaction remains unverified while the GUI hold is in force.
