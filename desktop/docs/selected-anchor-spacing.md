# Numeric selected-anchor spacing

New parent-selected final point-operation slice for dimensioned vector layouts.
Base PR67 `ade3cd40521c0b632e7ff3b251a070df41c25903`. Existing Distribute fixes
both ends; no numeric anchor interval exists at this base. Layer distribution
operates on a different selection and is unchanged.

Add an explicit Set X spacing / Set Y spacing operation for2-10000 selected
anchors in one resolved editable source. Sort by artboard axis coordinate, with
exact ties ordered by resolved source contour/anchor traversal. Keep the minimum
anchor coordinate fixed and set the rest to `minimum + index * interval`.
The maximum moves. Zero interval superimposes selected axis coordinates.
Reject non-number, nonfinite, negative and overflowing intervals atomically.

The interval uses actual artboard document coordinates: SVG viewBox user units.
The current artboard-unit selector is a display/export conversion; it does not
rescale stored point coordinates. This field explicitly says document units and
does not relabel raw coordinates as mm/cm. No new unit setting or parser.

Keep the orthogonal axis, relative handles, IDs, contour order and unselected
data. Reuse the existing transformed Direct/inverse route, point identity guard,
numeric no-op precision and atomic command. Locked/driven/unsupported/singular/
stale selections reject as a whole. One changed operation is one Undo; no-op
preserves dirty/history/Redo. Input/change alone stages a transient value;
only explicit apply buttons commit. Escape discards the staged value. No new
shortcut, generic expressions, multisource or panel redesign.

Focused acceptance: two/many points, ties, zero/fractional/invalid intervals,
transforms, preserved geometry/identity, Undo/Redo/no-op and stale/guard rejection.
One representative fractional native CDP synthetic case includes JSON/SVG reload.
Native synthetic is distinct from physical/OS input and OS Save/Open/cold reopen.
Use the verified D Cargo target/runtime/output and child TEMP/TMP, retaining the
existing small C frontendDist. Preserve all earlier sealed outputs. No main
merge, release or Library upload.
