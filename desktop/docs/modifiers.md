# Non-destructive vector modifier catalog

These nodes preserve the upstream source and can be reordered by graph wiring. Use Add node
search. Development build required: the previously distributed 7496aee trial cannot load these
new node names. Native Windows interaction and a replacement trial package remain open.

| Node           | Controls                                                                                              | Current contract                                                                                                                                              |
| -------------- | ----------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Mirror         | Left-right / Up-down / Angle; Axis Center; Axis Angle; Keep Original                                  | Path before paint; live symmetric pair optional; no seam weld or raster reflection                                                                            |
| Corner Round   | Radius in px                                                                                          | Straight-edge corners only at nonzero radius; short edges clamp radius; open ends fixed                                                                       |
| Resample Path  | Points per Contour, 2–1024                                                                            | Arc-length samples become polylines; closed paths need 3; low counts change details                                                                           |
| Roughen        | Amount in px per axis; Seed; Keep Endpoints                                                           | Deterministic position noise; no new points; handles translate with anchors                                                                                   |
| Smooth Path    | Factor 0–1; Iterations 1–50; Keep Endpoints                                                           | Neighbor-average positions; can shrink/collapse outlines; not curve fitting                                                                                   |
| Trim Path      | Start / End fractions 0–1                                                                             | Per-contour length; Start ≤ End; equal values empty; partial closed inputs become open, no wrap                                                               |
| Set Path Start | Start fraction 0–1                                                                                    | Per-contour closed-path seam moves by arc length; open contours pass through; winding and exact cubic shape stay intact                                       |
| Subdivide Path | Segments per Edge 1–32                                                                                | Equal-parameter Bézier portions; exact curve shape/tangents retained; closed seam not duplicated                                                              |
| Expand Stroke  | Existing upstream Stroke width/cap/join                                                               | Solid straight-edge stroke → closed outline geometry; add Fill after; no type-preserving bypass                                                               |
| Simplify Path  | Tolerance in px                                                                                       | Straight-edge RDP reduction; fixed endpoints and valid-ring fallback; not topology repair                                                                     |
| Dash Path      | Dash Length; Gap Length; Pattern Offset in px                                                         | Actual cubic dash geometry; positive dash length; closed-seam fragments rejoined                                                                              |
| Reverse Path   | None                                                                                                  | Reverses contour direction exactly; swaps Bézier handles and retains each closed contour's first anchor seam                                                  |
| Pucker & Bloat | Amount −100..100%; Automatic Center or manual Center                                                  | Opposite anchor/control scaling; automatic center uses all anchor bounds; no Illustrator numeric parity claim                                                 |
| Wave Points    | Vertical / Horizontal; Amplitude ±100000 px; Wavelength 0.0001–100000 px; Phase ±360°; Keep Endpoints | Static sine displacement of existing anchors only; optional open-end pinning; relative Bézier handles unchanged; coordinate/wavelength ratio limited to ±1e12 |
| Taper          | Amount −50..50%; Horizontal / Vertical axis                                                           | Whole-input control bounds; scales the perpendicular dimension; nonlinear curve interior is approximate                                                       |
| Bend Points    | Angle −180°..180°; Horizontal / Vertical axis                                                         | Bends all contours into a circular strip around the shared control-bounds midpoint; folds are rejected; nonlinear curve interior is approximate               |

All new numeric fields use the common drag/type interaction. Bypass is available for the
Path→Path effects. Common Session commands provide Undo/Redo and locks, and invalid edits
preserve authored state. Output/work budgets fail explicitly rather than freezing on huge input.
Derived geometry is marked unstable so downstream Path Remap edits suspend on changed input.
Direct canvas text-character editing through these effects is unavailable; edit upstream Text.

## Useful stacks

- Polygon/Star → Corner Round → Mirror (Keep Original) → Fill
- Curve/Text → Resample Path → Roughen → Smooth Path → Fill or Stroke
- Curve → Trim Path → Stroke. A Fill on an open path uses an implicit closing edge; omit Fill
  when you want only the retained curve segment.
- Curve → Subdivide Path → Remap → Fill for denser, editable anchors without changing the curve.
- Curve → Resample Path → Stroke → Expand Stroke → Boolean → Fill
- Curve → Dash Path → Stroke for actual separated path pieces.
- Curve → Set Path Start → Trim Path / Dash Path to move where closed-contour traversal begins while keeping its forward direction.
- Curve → Reverse Path → Trim Path / Dash Path to reverse direction-sensitive start and pattern order.
- Imported polyline or Resample Path → Simplify Path for point reduction.
- Rectangle/Star → Pucker & Bloat → Fill for pointed or petal-like shapes.
- Line/Curve → Resample Path → Wave Points → Stroke for a static sine-like outline.
- Text/Path → Taper → Fill for size progression across a wordmark; Resample first for finer curve deformation.
- Line/Curve → Bend Points → Stroke or Fill for a circular strip; Resample first for finer deformation detail.
- Existing Geometry Repeater, Closed Path Offset, Transform, Boolean and Join Geometry remain
  available; these additions do not replace them.

Open `examples/modifier-playground.json` in this development build for editable examples. It has
41 evaluated editable layers and examples for all 16 listed modifiers. Rebuild with
`node scripts/modifier-playground.js`; it validates all layers and exports SVG. Detailed evidence,
research sources and limitations: [editing refinement](editing-refinement.md).
