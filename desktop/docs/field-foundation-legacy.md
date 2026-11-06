# Legacy Luminance point bridge

The internal `LegacyPointPathAdapterV1` is the first bounded part of the
[Field System Foundation](https://app.notion.com/p/40bfd279a6f3814ea121e16cadd46389).
It handles only the disconnected one-anchor contours produced by the existing
Luminance to Points node. The node converts its generated Path to a transient
SampleSet and immediately lowers it back to the same Path for the existing
Instance on Points input. No project schema, node port, paint mapping, Key Color
behavior, or authored data changes.

Each sample derives a deterministic ID from the original contour and point IDs.
Its position is copied without a coordinate conversion. The internal
`legacy-document-units-v1` marker identifies those existing document-unit
coordinates; it does not define a general Field Space or a saved value. Scalar
instance scale becomes a uniform pair, rotation is a degree Angle, and the
luminance and darkness values become typed Scalar attributes. Request-local
membership, geometry, ordering, and attribute descriptors distinguish changes
without establishing a persistent cache contract.

Lowering preserves contour and point IDs, order, coordinates, scale, rotation,
and both scalar values. It rejects unsupported sample attributes, nonuniform
scale, non-degree angles, stale descriptors, and Path shapes outside this bridge
instead of discarding data. Existing Luminance limits and zero-scale omission
remain at the original producer and consumer boundaries. This bridge is not a
general Path adapter, Field evaluator, InstanceSet, or Captured Color feature.

The fixed fixture in `tests/fixtures/legacy-luminance-v1.json` records the
pre-bridge Path, materialized fingerprint, SVG, and saved document. Focused tests
also cover conversion, rejection, save/reopen, Undo, and Redo.
