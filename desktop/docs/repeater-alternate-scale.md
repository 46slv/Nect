# 2D Repeater odd-copy scale

Base: PR57 `codex/text-wrap-rotation-resize`, verified remote
`c67b5647f4f5a80c4d7ea6a5888ca46a12b2a04f` in `46slv/Nect`.
No equivalent odd-copy scale exists. This bounded feature is selected by the
current delegated instruction; Notion REQ-170 Candidate is not approval evidence.

Add one Scalar `alternateScale`, labelled **Alternate Scale (%)**, to the existing
`ae-2d` Geometry Repeater. Default100 is identity. Finite range0.01–1000% uses the
existing percentage upper budget and a positive numeric lower bound. Legacy
linear/radial modes and their active controls retain their existing behavior.
Linear and radial examples here mean two configurations of the 2D affine mode.
No new expression/index syntax, color/shape variation or shape-local Repeater.

For zero-based rendered copy index i, independent of Offset and paint-order
reversal, keep the existing accumulated matrix B (integer/fractional offset plus
i steps). Odd1,3,5… uses B\*A, where A = T(anchor) S(alternateScale/100) T(-anchor).
Even copies and100% use B unchanged. Advance B by the original step only; never
accumulate A. Equivalently the odd copy scales around B(anchor), preserving that
copy's placement. Applying scale around the original global anchor after B would
move its placement and is excluded. Positions use the full matrix; relative
handles use its linear part. Source, IDs/groups/holes, opacity and copy order stay.

Use the shared parameter/Session editing and evaluation checks. Old documents
backfill100 without a version bump or output change. Authored nondefault scale,
expression or connection requires/promotes schema45; retagging it older rejects.
Schema45 requires the control. History retains the complete authored descriptor;
failed fallback/resolved values must not mutate document/history. No dither or
other feature migration changes.

Focused verification:24-copy linear/radial seed geometry; default/even identity;
odd positions/handles around each copied anchor; positive/negative/fractional
Offset and reversed copy order; existing opacity/partial tail; source/ID stability;
atomic invalid literal/expression/wire updates, locks; Undo/Redo, JSON and SVG.
Inspect the actual property editor and representative native Tauri/WebView2
synthetic interactions. Exercise the existing PNG renderer if available without
OS-dialog claims. Build and seal a separate candidate; preserve earlier artifacts.
Physical input, OS dialogs/cold reopen, unrelated full audits and AE compatibility
are not implied. No protected-main merge, release or Library upload.
