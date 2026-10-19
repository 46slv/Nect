# Composition session owner

`CompositionSessionOwner` is the state boundary for a multi-Composition project.
`composition-session-owner-core.js` supplies the existing core `CommandSession`,
`History`, validator and clone policy. The owner projects only the active
Composition to that `CommandSession`, then records each committed edit back into
one project `History` snapshot. The snapshot contains every Composition and the
shared font/image asset registries; core History keeps embedded asset bytes
shared between snapshots.

Undo/Redo is project-wide and chronological. After editing A then B, Undo first
restores B, then A; Redo reapplies them in the same order. Asset additions are
part of that same project snapshot, so an edit Undo in B retains a font or image
previously added in A. Switching tabs does not change the project revision or
undo/redo availability. Unknown Composition IDs and invalid project candidates
are rejected before replacing the current session. Composition create, rename,
resize and delete each use one project History step. The active CommandSession
does not retain a duplicate local undo stack.

Owner orchestration tests inject small History/Session doubles, so they run
without the app's external packages. The core factory is a thin binding to the
real implementations; this checkout cannot load that binding because required
packages are missing. The app still needs to route document commands and
Undo/Redo through the core factory before it can expose Composition tabs or
persist the multi-Composition envelope. Native Open/Save, recovery, export,
variable-font activation and UI selection/view state are not connected here.
