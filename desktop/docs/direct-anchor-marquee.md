# Direct anchor marquee

New production slice selected by the parent under the user's feature-extension
request, not a historical adopted requirement. Source brief:
https://app.notion.com/p/401fd279a6f381a69a34e32f0d0ee85d.
Verified base: PR64 `3b56815f39b90147b1e5b8ad648891eb826b4cba`.
The current Direct blank branch clears point selection; no anchor marquee exists.
Whole-artwork/Graph marquees remain separate.

Primary blank-Canvas drag in Direct selects anchors in the currently resolved
single editable source. Existing point/handle/segment/guide hit routes win first.
Four CSS pixels start the rectangle. Smaller motion keeps the existing blank
click. Replace is default; Shift unions the starting point selection, never
toggles per move. Alt has no new meaning. Neither other sources nor whole layers
are added.

Use `directPointContext`/`directSelectionSource`'s transformed artboard path and
only `pointControlTargets` position entries. Test inclusive anchor centers against
the artboard-space rectangle from the inverse Canvas screen CTM. Handles, segment
intersections and layer bounds are not anchor hits. Clamp the endpoint to the
visible Canvas stage. All preview elements ignore pointer hit testing.

Preview only changes the rectangle/anchor rings. Release updates transient point
selection and its panel once. Preserve focus if still selected; otherwise choose
the first selected ID in stable source order. Locked/driven anchors are selectable;
existing atomic whole-selection edit guards remain unchanged. No geometry, schema,
dirty state, JSON or history change is authored by selection.

Snapshot Session/revision/composition/layer/source owner, route matrix/output and
source fingerprint, ordered path IDs, full point scope and artboard/capture
identity. Cheap event guards reject changed owners, revisions or detached views;
resolve and compare scope/fingerprint before commit or cancellation restoration.
Escape, pointercancel, lost capture and blur restore the starting selection/focus
only while identity still matches. Stale gestures discard their own overlay and
never restore over newer state. Late move/up cannot commit a retired gesture.

Acceptance is bounded model/DOM coverage for anchor-only/reverse/inclusive hits,
rotation plus nonuniform scale, zoom/pan, replace/Shift, threshold/routing,
locked/driven edit rejection, cancellation/late events and stale Session/source/
Path Remap identity, plus a few native CDP synthetic cases. Physical/OS input and
native Save/Open dialogs remain outside the available verification route.
