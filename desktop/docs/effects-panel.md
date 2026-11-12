# Effects panel — first Transform route

The fixed Effects drawer provides search and Apply for a selected artwork. Its first
entry inserts one identity Transform before the solid Fill of a single visible,
unlocked Circle, Ellipse, Rectangle, Polygon, Star, or unattached Text artwork
whose output is `Path → Fill → Output`. Other routes, shared consumers, locked
inputs and the node limit are refused; the existing graph is not rewritten.

The panel captures the selected artwork and Session/project identity at display
time. Apply rechecks that state, and the Session replans the exact typed edge on a
detached candidate before one History commit. The new card occupies an unused
graph column; existing card positions, node and point IDs, paint, and unrelated
connections remain intact. The result opens in the existing Inspector. Normal
Save/Open stores ordinary nodes and connections.

This is a partial REQ-22 route, not a complete effect library or Macro workflow.
The existing named node presets retain their detached snapshot behavior; they are
not registered Macro instances. Macro publication, controls, and cross-document
reuse remain separate work.

The focused browser test covers search, Apply, initial paint, Inspector editing,
canvas movement, Undo/Redo, Save/Open, and stale controls. Workspace and Session
tests cover selection, composition, unavailable routes and guarded reentry.
Native WebView2 and physical IME acceptance are not established by those tests.

## Connected Macro Session slice

The Session can insert a registered, revision-pinned two-Transform Macro into a
single `Rectangle → Transform → Fill → Output` route and edit its published Scalar
overrides. The existing terminal Transform remains the artwork Move target. The
connected Path is evaluated before commit; an overflowing insertion or affected
upstream edit is rejected without changing the Project or History. Undo/Redo and
Project save/reopen retain the instance and its definition pin.

This is not yet an Effects-panel workflow: definition creation, library access,
published controls and Macro Apply UI are not connected. Partial-node copy,
node presets and Composition import containing Macro instances are rejected until
their definition closure is implemented. A complete Layer duplicate or same-Project
Layer snapshot keeps its pinned definition. Native acceptance remains unverified.
