# Selected anchor alignment

New parent-selected production slice under the user's feature-extension request,
not a historical adopted requirement. Verified base: PR65
`b918852cd558e4cb46521b3809237b39c3c9a33b`. Existing point editing has common
translation; existing alignment acts on artwork/node outputs, not selected anchors.

Add six small controls to the existing selected-point panel: X/Y min, center and
max, labeled **Align anchors / Artboard**. Operate on at least two distinct
selected IDs in the current single resolved editable source. Center is the
selected coordinate range midpoint, not the arithmetic mean. Other points,
relative handles, contour/unit IDs, holes and open/closed state remain unchanged.

Resolve with `directSelectionSource`, measure transformed artboard anchor
positions, and map each world-axis correction through existing
`inverseAffineDelta`. Rotation/nonuniform scale are supported; singular or
unsupported routes reject. Fresh route/ordered IDs/fingerprint and Session/
revision/owner checks protect the action. A stale selection or one locked/driven
member rejects the whole operation, including a member requiring zero motion.
No bake, automatic rebind or persistent model/schema additions.

Reuse `translate-points` and its private point-candidate implementation, adding
validated ID-keyed per-point deltas. Compute one candidate and run existing final
validation once; do not dispatch separate commands or clone/evaluate once per
anchor. `align-points` is the semantic planner entry. One changed operation is one
Undo; no-op retains dirty/history/Redo. Sparse Remap entries requiring zero motion
retain their exact prior representation.

No-op detection uses the existing geometric alignment floor of 1e-9 world units,
expanded only for the existing inverse edit mapper's ten-decimal rounding and
floating-point scale: row norm times1e-10 plus16 machine epsilons of world extent.
This policy prevents repeated alignment of already aligned transformed points
from creating numerical-only history. First nonzero corrections are not rounded
by the planner; the shared inverse edit mapper keeps its existing precision.

Acceptance: all six actions including center[0,10,30]=15; unchanged unselected
points/handles/IDs; transformed and Remap sources; no-op, Undo/Redo, JSON/SVG
roundtrip; invalid, stale, locked/driven and singular rejection without partial
edits. Focused model/DOM and representative native CDP synthetic interaction;
actual OS input/save dialogs/normal clean cold reopen remain unverified. No
distribution, new shortcut, multiple-source alignment or panel redesign.
