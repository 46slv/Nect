# Selected anchor even spacing

New parent-selected slice for reducing manual point spacing in Shiro's vector
work. Base: PR66 `3b04ffb967c86ece95c412d91622fc1a0f22262f`.
Current selected-point controls have common delta and six alignments. Existing
Distribute commands operate on layer IDs, not selected anchors; no equivalent
point operation exists at this base.

Add Horizontal/Vertical even-spacing controls in the existing point-selection
pane. Require 3-10000 distinct selected anchors in one resolved editable source.
Sort by the chosen artboard-axis coordinate, breaking exact ties with the original
resolved contour/anchor traversal index, independent of click/selection order.
Keep both extreme endpoints fixed. Place each internal point at
`min + (max - min) * index / (count - 1)` (equivalently `min + step * index`).
The other axis, relative handles, unselected points, IDs and contour order stay
unchanged. All-equal coordinates are a no-op.

Reuse PR66's transformed Direct route, inverse affine edit mapper, numeric no-op
precision, source/fingerprint/Session/revision guards and ID-keyed atomic point
candidate. Locked/driven endpoints must reject too, even though their delta is
zero. One changed operation is one Undo; no-op preserves dirty/history/Redo and
sparse/held Remap representation. No schema, bake/rebind, new shortcuts, distance
input, multisource or curve-length distribution.

Acceptance: three/many points, ties and reversed selection order, negative values,
both axes, transformed sources, unchanged endpoints/other coordinates/handles/
unselected data/order, all-equal and repeat no-op, one Undo/Redo, stale/invalid and
whole-selection guards, JSON/SVG reload. Focused model/DOM plus a few native CDP
synthetic cases; do not claim physical/OS input or OS Save/Open.

Heavy build artifacts, verification output and child-process TEMP/TMP use the
verified dedicated D output root. The unchanged Tauri `frontendDist=../dist`
still requires the small C frontend output, gated on available space. This is the
first D Cargo/native execution, not an already verified environment. No global
configuration/guard changes, old output deletion, main merge, release or Library
upload. Record each stage's actual result and any capacity-local hold.
