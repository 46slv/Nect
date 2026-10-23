# Rotated Text whole-artwork wrap-width handle

Base: verified PR56 remote `04b5b802002437999de763e7652dee16f63efb25`,
`codex/text-wrap-cross-reference-resize` in `46slv/Nect`.
The existing helper rejects rotation; source-mode controls are pre-downstream.

Support only the existing regular horizontal auto-height Text capsule through
one static Transform and solid Fill/Output. Reuse exactly the evaluator/renderer
`transformValues().matrix` from `transform-affine.js`: anchor maps to
Position+Offset; rotation and positive scale are already composed there. Do not
re-apply a pivot or infer a correction from screen-X/cos(angle). Preserve the
existing positive X/Y scales at zero rotation; admit their rotation without skew.
Skew, reflected/near-zero scales, nonfinite/noninvertible matrices, unsupported
routes/fonts, vertical/multiple Text and general artwork resize remain excluded.

Compute the same logical Text frame and X/Y references in node-local coordinates,
then transform corners/handle by that matrix. Canvas CTM inverse maps pointer to
world; the existing full affine inverse maps its delta to node-local. Translation
cancels in a delta, equivalently inverse(pointer)-inverse(start). Only local X
changes wrap width, including at90 degrees; local Y motion is a no-op. Keep the
existing center2x/end-negative factors,0.01-100000 range, coalesced preview,
one Undo/cancel and identity/revision guards. Map the cursor to the displayed
width-axis direction; this affects the cursor only, never the width calculation.

Existing `inverseAffineDelta` rejects nonfinite/singular/ill-conditioned transforms.
Check effective positive scales and zero skew before exposing a control; reject
nonfinite frame points. Invalid deltas fail before Session update, and finite
large pointer excursions clamp to width bounds. No authored Position, rotation,
anchor, characters/IDs or selection changes; numeric width JSON/SVG is canonical.
No core/model/schema or text-layout change, bake/rebind or generic resize work.

Bounded verification: zero/90/negative/arbitrary angles in model, three rotated
DOM boundaries, nonorigin anchor/Position, zoom/pan, local-Y no-op, numeric
equivalence, preserved transform/identity, Undo/Redo/cancel and unsupported guards.
Native synthetic uses two representative rotations/zooms and center/end refs,
not the whole past matrix. Preserve all earlier candidates and PR55 proof; no
unrelated full audit, main merge, release or Library upload. Physical input,
actual OS focus loss, OS dialogs/cold reopen and IME remain separate coverage.
