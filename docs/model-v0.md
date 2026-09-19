# Native Document Schema v0

## M0 vocabulary

Implemented: Document, Composition, Artboard, Group, Path, Contour, Point, Scalar, Binding, Collection.

Layer is the UI presentation of an Object; there is no duplicate Layer state model.

Text, Raster, Resource, Operator, Instance and full compositing are not stubbed ahead of real callers.

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

## Persistence

Native JSON stores authored data only, not evaluated caches, Qt widgets, or session revision.

Unknown fields/versions/kinds, duplicate JSON keys, invalid references, non-finite values, unsupported color/unit claims are rejected explicitly.

Current M0 limits are safety bounds, not product performance targets.

## Next contracts

Add Text only with real shaping/layout requirements.
Add Raster only with immutable source/provenance and explicit color semantics.
Add Operator/Instance only with identity and regeneration contracts.
Add compositing only after alpha/group/color-space semantics are defined.
