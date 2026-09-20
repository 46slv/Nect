# Native Document Schema v0

## M0 vocabulary

Implemented: Document, Composition, Artboard, Group, Path, Contour, Point, Scalar, Binding, Collection, retained Circle/Rectangle sources and Point Edit.

Layer is the UI presentation of an Object; there is no duplicate Layer state model.

Text, Raster, Resource, general operator stacks, Instance and full compositing are not stubbed ahead of real callers.

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

The writer emits 0.2; the reader accepts strict 0.1 and 0.2. Migration of 0.1
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

## Next contracts

Add Text only with real shaping/layout requirements.
Add Raster only with immutable source/provenance and explicit color semantics.
Add Operator/Instance only with identity and regeneration contracts.
Add compositing only after alpha/group/color-space semantics are defined.
