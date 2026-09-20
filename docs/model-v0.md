# Native Document Schema v0

## M0 vocabulary

Implemented: Document, Composition, Artboard, Group, Path, Contour, Point, Scalar, Binding, Collection, retained Circle/Rectangle sources, Point Edit and local Fill/Stroke/Repeater stacks.

Layer is the UI presentation of an Object; there is no duplicate Layer state model.

Text, Raster, Resource, general node graphs, addressable generated Instances and full compositing are not stubbed ahead of real callers.

## Identity / ownership

IDs are opaque, document-unique strings. Names and array indices are not identity.

Rename and point reorder do not retarget references.

Objects are owned exactly once by a Composition root or Group child. Ownership is derived from roots/children, not duplicated in a parent field.

Collections are non-owning ordered sets. Collection membership does not alter the source object's parent or transform inheritance.

## Coordinates / handles

M0 stores `du96` (1/96 inch), Y-down coordinates, clockwise-positive degrees.

Each point stores X/Y plus incoming/outgoing handle angle and length as addressable Scalars.

Polar handle data is canonical in M0; Cartesian handle vectors are derived. A zero-length handle still retains its authored angle.

Transform is a six-value affine matrix. Do not also store decomposed TRS as another authority.

## Binding

Current subset:

`evaluated = evaluate(source_ref) * scale + offset`

Bindings use stable IDs. Name lookup is only an authoring convenience that resolves to an ID.

Driven properties reject direct Set until explicitly unlinked. Unlink freezes the current evaluated value into the literal.

Bindings must have compatible units and may not form cycles.

General expressions later compile into the same typed property/dependency owner; do not create a second JavaScript document state.

## Session

Session owns committed mutation.

A batch is applied to a candidate document and validated before commit. Failure preserves both the original document and revision.

Undo/redo revisions are monotonic. M0 uses snapshot history with a small fixed limit; this is not the final large-document strategy.

M1 adds CreatePath, AddPoint, RemovePoint, CloseContour, DeleteObjects and
ReorderObjects to the same command boundary. Deleting a referenced property is
rejected unless dependent targets are explicitly unlinked/frozen in the atomic
batch. Collection members of removed objects are pruned without reparenting.
Creation/reorder does not use array positions as identity. These commands do not
change the native 0.1 schema.

Session owns an optional gesture preview evaluated from the committed starting
snapshot. Committed reads/save/recovery retain the start document until commit;
the Canvas uses `preview_document`. Other mutations and history navigation reject
while a gesture is active. Cancellation or returning to an empty preview creates
no revision/history; successful completion commits once. Failed previews keep the
last valid preview and never partially commit.

## Persistence

Native JSON stores authored data only, not evaluated caches, Qt widgets, or session revision.

Unknown fields/versions/kinds, duplicate JSON keys, invalid references, non-finite values, unsupported color/unit claims are rejected explicitly.

Current M0 limits are safety bounds, not product performance targets.

## Native 0.2: retained primitives and point corrections

The primitive slice introduced 0.2; the current writer emits 0.5 and the reader
accepts strict 0.1 through 0.5. Migration of 0.1
preserves authored values, IDs and bindings, with no geometry conversion. The
historical linked fixture in `tests/fixtures/native-v0.1-linked.nect` is loaded,
edited, saved and reopened in separate processes. Unknown fields and behavior
versions remain errors. See `schemas/native-v0.2.schema.json`.

A Path owns either authored contours or one retained primitive source, never
both. `nect.shape.circle` and `nect.shape.rectangle` version 1 map typed local
distance parameters to a closed cubic path. Source instance IDs are stable;
derived contour and point IDs append fixed semantic roles (east/south/west/north
or corner names). Source IDs have a 64-character bound; generated IDs fit the
normal 96-character bound. The correction instance namespace is reserved.

Circle has center_x, center_y and radius; Rectangle has center_x, center_y, width
and height. `generator.*` parameters are ordinary linkable distance properties.
Derived point/handle fields join the same dependency evaluator; generator/point
cycles reject atomically. Circle uses four cubic arcs, with handle length
`radius * 0.5522847498307936` (a cubic approximation, not an exact rational circle).

Setting or linking a generated point field creates/reuses the visible downstream
`nect.path.point-edit` version 1 instance. Only changed fields are stored, as
**absolute local scalar overrides**, not a duplicated evaluated path. Unspecified
fields continue to follow the generator. Disabling Point Edit retains authored
overrides and evaluates the generator. Editing while bypassed re-enables the
correction instance. Topology editing requires explicit conversion. Unmapped
corrections reject; they are never silently assigned by array index.

`get`/`properties` distinguish authored, generated, point_edit and
bypassed_point_edit origins; generated fallback has `authored: null` and a
separate evaluated value. Native save stores no derived anchors. SVG and Canvas
read the same evaluated point values.

`convert_to_path` preserves object, contour and point IDs and active point
bindings. It freezes generator-derived values and removes source/Point Edit;
bypassed corrections are discarded explicitly in the conversion plan. Surviving
references to generator-only properties block conversion until the caller
explicitly unlinks/retargets them. `conversion_plan` is read-only; the mutation
still checks the expected revision. Undo restores the complete procedural state.
Conversion changes the geometry source only; later Fill/Stroke/Repeater operations
remain authored and editable.

## Native 0.3: ordered local Shape stack

A Path has one ordered `stack` of stable operation instances. Supported version 1
types are `nect.paint.fill`, `nect.paint.stroke` and `nect.shape.repeater`. Each
has an enabled/bypass flag and ordinary scalar parameters addressed as
`op.<instance-id>.<parameter>`. Reorder never changes identity or links. Deletion
that would break a surviving reference fails atomically. `operator_types` exposes
exact creation templates/units; `render_plan` exposes evaluated paint grouping.

The 0.1/0.2 Stroke becomes one real stack instance, with a deterministic unused
document-unique ID. The `legacy_stroke` string retains the old `stroke.*` address
as an alias to that instance; it is not another copy of color/width data. All old
authored Scalars and Binding references survive, including a fixture where an
existing ID collides with the preferred migration instance name. Native 0.3 stores
only the stack. New property discovery uses canonical operation addresses.

The pure core `evaluate_shape` owns ordered geometry/paint semantics, producing
immutable shared cubic contours, transformed path instances and ordered paint
layers. Canvas and SVG lower this same result. Neither renderer implements an
independent Repeater. Source handles edit the original geometry; clicking a
virtual copy selects its owning object. Copies are not individually editable
authored objects or addressable point identities.

Scope/order follows the bounded AE counterpart:
- Fill/Stroke apply to paths preceding them. Earlier paint entries appear above
  later ones by default (`composite: below`); explicit `above` overrides this.
- Repeater duplicates preceding paths **and** their paint layers. Before paint,
  copies are painted as one compound path; after paint, each copy is painted
  independently. Nested Repeaters compose these results.
- Repeater uses integer Copies (0–1000), a fractional transform Offset, fixed-step
  position/rotation and positive multiplicative X/Y Scale. Each copy's transform
  uses copy index + Offset around the local Anchor. Composite controls copy order;
  Start/End Opacity interpolates across existing paint copies. A Repeater before
  any paint has no paint opacity to alter. No fit-to-arc mode is implied.
- Fill supports nonzero/evenodd. Stroke uses butt caps and miter joins (limit 4).
  Width zero is invisible. Color is sRGB RGBA, normal source-over compositing.

Limits are explicit: 128 entries, 4096 generated path instances, 8192 paint layers
and 250000 expanded cubic anchors per object's geometry/paint result. Invalid
parameters, unsupported versions/options, nonfinite transform output and excessive
expansion reject the whole mutation. Bypass retains authored parameters and links.
Group-local stacks, arbitrary path operations, dashes, per-paint blend
modes and full AE interchange remain unsupported at this checkpoint.

Behavior reference: [Adobe shape paint/path operations](https://helpx.adobe.com/after-effects/desktop/drawing-painting-and-paths/shapes-and-shape-attributes/shape-attributes-paint-operations-path.html).

## Native 0.4: retained gradients

Fill/Stroke optionally own a version 1 gradient with document-unique component
and stop IDs. Linear and centered radial gradients use local start/end coordinates;
radial radius is their distance. The original solid RGB remains authored as a
fallback; paint alpha is overall opacity and multiplies each stop's alpha once.
The gradient enabled flag switches to Solid without deleting stops or links.
`set_gradient` creates/replaces/removes the component atomically; callers must
retain existing IDs and Scalars when editing its structure. `gradient_types`
provides exact creation templates. Native 0.3 files migrate without other changes.

The four coordinates and stop offset/RGBA channels are ordinary linkable Scalars:
`op.<paint>.gradient.<gradient>.start_x` (also start_y/end_x/end_y) and
`op.<paint>.gradient.<gradient>.stop.<stop>.offset` (also r/g/b/a). Coordinates
are distances; offsets/colors are scalars in [0,1]. Reordering stops never retargets
a reference. Removing a referenced stop/component rejects until surviving links
are explicitly frozen or retargeted. Undo and gesture preview use the same Session.

Authored stop order is preserved; evaluation sorts by offset. There are 2–64 stops,
with distinct evaluated offsets, including when bypassed. An enabled gradient
on an enabled paint requires start/end distance greater than 1e-9. Pad spread and
independent sRGB component/alpha interpolation are explicit. Coincident hard edges,
radial focal offsets, repeat/reflect spread and alternate color spaces are not
yet supported. Qt 6.5.3 [replaces coincident stops](https://github.com/qt/qtbase/blob/v6.5.3/src/gui/painting/qbrush.cpp#L1475-L1527);
rejecting ties prevents a silent loss between authored state and Canvas rendering.

Canvas uses QGradient ComponentInterpolation, with original-source start/end
handles. SVG uses userSpaceOnUse, sRGB and pad gradients. Repeating after paint
transforms its gradient with every copy; repeating before paint shares one gradient
over compound paths. `render_plan` exposes the same resolved stops and coordinates.

## Native 0.5: ordered output frames and parent size

An Artboard remains an output rectangle on its owning Composition's plane. Its
stable ID is independent of vector position and display name. `add_artboard`,
`update_artboard`, `delete_artboard` and `reorder_artboards` are atomic Session
commands. Reorder changes page/export order only; x/y/size edits change the crop
only. No Artboard operation moves artwork or changes object ownership. The
`artboards` read operation returns ordered authored/evaluated frame pairs.
Changed IDs include frames whose inherited dimensions change, not just their
owning Composition. Deleting the last frame through commands rejects explicitly.

Optional `parent_size: {artboard, width, height}` references a frame in the same
Composition. Each Boolean chooses whether that dimension follows its parent;
false keeps the frame's local width/height. The retained local values are valid
fallbacks, not a second evaluated authority. Reset Override means enabling the
corresponding Boolean. `detach_artboard_parent` freezes both effective dimensions
and removes the reference. Position/name never inherit. Chained parents are
supported; cycles, cross-plane references and deletion of a referenced parent
reject atomically. A caller may detach/retarget children and delete their parent
in one explicit batch. Limits: 1024 frames per Composition, parent depth 256.

This is the size-inheritance foundation of Parent Artboards, not reusable content
or logo/guide/page-number inheritance. Those require the shared definition/instance
contract and remain pending. Native 0.1–0.4 frames migrate with no parent binding;
their coordinates, order, authored artwork and SVG output remain unchanged.

## Next contracts

Add Text only with real shaping/layout requirements.
Add Raster only with immutable source/provenance and explicit color semantics.
Add Operator/Instance only with identity and regeneration contracts.
Add compositing only after alpha/group/color-space semantics are defined.
