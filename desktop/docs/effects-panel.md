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

The selected effect can be added to or removed from Favorites with the star
button. Favorites use the effect's stable entry ID and appear first in search
results. They are workspace preferences in local storage, not Project edits or
History entries. Unavailable effect IDs are retained; unreadable preferences or
failed writes do not replace the saved list. Applying a favorite still uses the
same selected-artwork and Session checks as every other effect.

This is a partial REQ-22 route, not a complete effect library or Macro workflow.
The existing named node presets retain their detached snapshot behavior; they are
not registered Macro instances. Cross-document Macro reuse remains separate work.

The focused browser test covers search, favorite persistence, Apply, initial paint, Inspector editing,
canvas movement, Undo/Redo, Save/Open, and stale controls. Workspace and Session
tests cover selection, composition, unavailable routes and guarded reentry.
Native WebView2 and physical IME acceptance are not established by those tests.

## Project-local Macro slice

Select two connected, unlocked literal Path Transform nodes and use Create Macro
in Presets to name the definition and publish one or more Scale or Rotation
controls. The definition is stored in the current Project. Connected, expressed,
or extended Transform inputs are not captured by this first slice.

The Effects drawer lists the Project's registered definitions by stable ID and
revision. Applying one to a supported Rectangle adds a single Macro instance;
the Inspector edits or resets its published Scalar values. List numbers distinguish
entries without changing their stored names or underlying identities.

The Session can insert a registered, revision-pinned two-Transform Macro into a
single `Rectangle → Transform → Fill → Output` route and edit its published Scalar
overrides. The existing terminal Transform remains the artwork Move target. The
connected Path and affected downstream Path outputs are evaluated before commit;
an overflowing insertion or affected edit is rejected without changing the Project
or History. Undo/Redo and Project save/reopen retain the instance and its pin.

Partial-node copy, node presets and Composition import containing Macro instances
are rejected until their definition closure is implemented. A complete Layer
duplicate or same-Project Layer snapshot keeps its pinned definition. If that
definition leaves History, a copied Layer cannot silently adopt a different
definition with the same ID and revision. An external library and cross-Project
reuse are not implemented. Native acceptance remains unverified.
