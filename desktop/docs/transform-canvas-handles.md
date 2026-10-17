# Transform canvas scale and rotation

Open [the editable typography example](../examples/transform-canvas-handles.json). Its selected Transform controls the joined headline and subtitle. The square **Scale** handle changes its existing Scalar scale uniformly; the round **Rotate** handle changes its existing rotation in clockwise degrees. Drag elsewhere on the canvas for the existing Offset move. Select exactly one Transform to show these controls. No new parent or Transform is inserted.

## Exact editing stage and pivot

The dashed path and brown controls describe **Selected Transform output**, before downstream nodes. This is stated on the canvas and in the Inspector. With a direct Fill/Stroke/Output chain the selected outline coincides with final path geometry. A downstream Transform, Repeater, Remap, Boolean or another branch may produce different final artwork; the controls do not claim to be a final-output bounding box or solve that graph backward. Other layers remain visible normally.

Pivot is evaluated **Position + Offset** (legacy documents without Position use Anchor as Position). The shared equation is `p′ = position + offset + L × (p − anchor)`, where L includes rotation, signed width/height scales and skew. A gesture only writes Scale or Rotation. Anchor, Offset, child positions, character offsets, text size and spacing remain unchanged. Pivot never recenters itself on bounds. Input corners are used only to place a scale-control ray. The affine helper is shared with character provenance, not a new transformation convention.

The control ray uses the input-bounds corner farthest from Anchor. Its square normally sits on that transformed corner. For tiny/zero scale it stays at least 40 screen pixels from the pivot; rotation sits another 30 pixels out. These padded controls remain usable when the artwork collapses. Their marker sizes stay constant on screen as the view zooms. A degenerate input at the pivot uses a 100-document-unit reference ray; an empty/disconnected input shows no controls.

## Gesture behavior

Scale is the signed projection of pointer movement along the starting ray, divided by its unscaled input radius. It never divides by the current Scale. Existing negative values are preserved on load: pulling away from the pivot increases their magnitude; crossing through zero can change sign. Zero and near-zero values can be expanded again. Scale is still one Scalar; no separate width/height, image resize or multi-selection resize is introduced.

Rotation accumulates shortest angle differences across ±180° and supports multiple full turns. Hold Shift while moving to snap the resulting absolute Rotation to multiples of 15°. The first three screen pixels are click tolerance. Movement within four screen pixels of the pivot is ignored to avoid undefined angles. Scale/Rotation do not attract translation guides; their numeric fields remain available for exact values.

Middle or Space drag pans, even over these controls. Coordinate conversion uses the artboard screen CTM, including zoom and pan. Connected, expression-driven or locked Scale/Rotation individually remain read-only at evaluated positions; the other scalar may still be edited. Layer lock blocks both. No source write-through or fallback modification occurs. The resolver is the same connection → expression → direct-value path used by geometry evaluation.

One completed gesture is one Undo entry. Click jitter and an unchanged out-and-back gesture preserve exact original floating-point values and add no entry. Escape, pointer cancellation, lost capture and window blur restore the entire pre-gesture document. Numeric drafts commit before the gesture snapshot, as a separate Undo entry. While the gesture owns its snapshot, Delete, Undo, tool switches and file shortcuts do not interleave a second action. Modifier state still controls rotation snapping.

## Persistence, exports and verification

There is no new document schema or stored handle state. JSON saves the same existing Transform parameters. SVG and PNG export evaluated artwork only; neither the outline, pivot, handles nor labels are exported. Save/reload preserves negative values and editable children.

- `transform-handles.test.js`: shared affine/pivot, signed scale, zero/near-zero recovery, angle boundary and turns, exact no-op, evaluator precedence/cache and stage independence.
- `transform-handles-dom.test.js`: actual UI pointer path, 1 Undo, interruption rollback, pointer identity, keyboard action isolation, affine CTM, pan priority, locks/drivers, child invariance, multiple-selection gating, hidden layers and JSON/export isolation.
- `transform-handles.e2e.js`: real Chrome typing-to-handle flow, uniform scale and Shift rotation, pan/zoom, negative/zero recovery, exact no-op, capture/cancel/blur/Escape, locked/driven values, JSON/SVG/PNG downloads and identical PNG bytes without the overlay. It records an editor screenshot and artboard screenshot.

Unit/DOM/build checks do not establish real-browser success. This local cloud environment cannot start Chromium because its socket operation is denied. The Windows CI must run the full browser suite and NSIS packaging for the exact published commit; native WebView2, high-DPI and OS-dialog acceptance stay separate.

## Reposition Anchor without moving artwork

Select a Transform and use **Reposition anchor** in the Inspector or node card. The nine presets use the exact combined input path bounds (including Bézier extrema), before this Transform, with corners, edge middles and center. Drag the canvas **Anchor** crosshair for a custom pivot. This is a rotation/scale pivot tool, not path-point deletion. The marker is in selected Transform output coordinates, before downstream processing; pan, zoom and screen CTM are converted before inverse affine mapping.

Both routes atomically write Anchor and compensated absolute Position: `position′ = position + L × (anchor′ − anchor)`. Offset and all upstream geometry stay unchanged. Subsequent rotation or scale uses the new pivot. Legacy documents gain Position only on a real change. Signed/nonuniform scales and skew use the full evaluated L. Presets remain valid at zero scale; a collapsed or ill-conditioned affine refuses direct dragging because inverse mapping is unavailable.

Anchor and Position must both be unlocked and undriven. A locked layer, bypassed Transform or empty input refuses the operation without changing anything. Drivers on other controls are evaluated, never rewritten. Preservation is for the current evaluated state; future upstream animation or driven scale/rotation can naturally change the result. A preset is one Undo entry; a completed drag is one Undo entry. Exact return to the press point and clicks add none. Escape, pointer cancellation, lost capture and window blur use the existing gesture rollback.

`transform-anchor-reposition.test.js` covers presets, affine invariance, legacy/absolute Position, retained Offset, signed/nonuniform/zero scales, skew, serialization, atomic refusals and gesture history. `transform-handles-dom.test.js` additionally exercises actual preset buttons and pivot pointer events under an affine screen CTM, downstream stage distinction, cancellation, no-op, Position locks and collapsed-scale rejection. These are headless checks; native/browser acceptance remains separate.
