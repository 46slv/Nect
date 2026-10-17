# Non-destructive vector modifier catalog

These nodes preserve the upstream source and can be reordered by graph wiring. Use Add node
search. Development build required: the separately frozen 19-modifier trial does not contain
Instance on Points. Native Windows interaction acceptance for this advancing source remains open.

| Node               | Controls                                                                                                                      | Current contract                                                                                                                                                                                                                                                                                                                                                                                        |
| ------------------ | ----------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Mirror             | Left-right / Up-down / Angle; Axis Center; Axis Angle; Keep Original                                                          | Path before paint; live symmetric pair optional; no seam weld or raster reflection                                                                                                                                                                                                                                                                                                                      |
| Corner Round       | Radius in px                                                                                                                  | Straight-edge corners only at nonzero radius; short edges clamp radius; open ends fixed                                                                                                                                                                                                                                                                                                                 |
| Resample Path      | Operation: Count or Spacing; Points per Contour (2–1024) or Maximum spacing (0.0001–100000 px)                                | Count preserves the existing per-contour sample count. Spacing rounds each contour up to whole equal arc-length intervals, so every gap is at most the requested maximum; it is a numeric approximation, not exact Blender Length parity. Open endpoints stay fixed; closed seams are not duplicated; low density can change details                                                                    |
| Roughen            | Amount in px per axis; Seed; Keep Endpoints                                                                                   | Deterministic position noise; no new points; handles translate with anchors                                                                                                                                                                                                                                                                                                                             |
| Smooth Path        | Factor 0–1; Iterations 1–50; Keep Endpoints                                                                                   | Neighbor-average positions; can shrink/collapse outlines; not curve fitting                                                                                                                                                                                                                                                                                                                             |
| Trim Path          | Start / End fractions 0–1                                                                                                     | Per-contour length; Start ≤ End; equal values empty; partial closed inputs become open, no wrap                                                                                                                                                                                                                                                                                                         |
| Set Path Start     | Start fraction 0–1                                                                                                            | Per-contour closed-path seam moves by arc length; open contours pass through; winding and exact cubic shape stay intact                                                                                                                                                                                                                                                                                 |
| Set Closure        | Closed                                                                                                                        | Sets actual closure on every contour; preserves anchors and relative handles; no endpoint weld; Fill's implicit edge remains separate                                                                                                                                                                                                                                                                   |
| Subdivide Path     | Segments per Edge 1–32                                                                                                        | Equal-parameter Bézier portions; exact curve shape/tangents retained; closed seam not duplicated                                                                                                                                                                                                                                                                                                        |
| Expand Stroke      | Upstream Stroke width/cap/join; Start / End width multipliers 0–4 (default 1)                                                 | Solid straight or bounded cubic centerline → closed outline geometry; equal multipliers retain constant width; unequal values ramp linearly by normalized centerline arc length on open contours and reject closed contours; 0.1 px centerline flatten tolerance; add Fill after; no type-preserving bypass                                                                                             |
| Closed Path Offset | Amount; Join (miter / bevel / round); Miter Limit                                                                             | Offsets explicitly closed nonzero regions. Explicit groups offset independently and keep overlap separate; ungrouped geometry (including Compound Text) is one global region collection. Positive expands, negative insets; fully eroded output may be empty. Place before paint.                                                                                                                       |
| Simplify Path      | Tolerance in px                                                                                                               | Straight-edge RDP reduction; fixed endpoints and valid-ring fallback; not topology repair                                                                                                                                                                                                                                                                                                               |
| Dash Path          | Dash Length; Gap Length; Pattern Offset in px                                                                                 | Actual cubic dash geometry; positive dash length; closed-seam fragments rejoined                                                                                                                                                                                                                                                                                                                        |
| Reverse Path       | None                                                                                                                          | Reverses contour direction exactly; swaps Bézier handles and retains each closed contour's first anchor seam                                                                                                                                                                                                                                                                                            |
| Pucker & Bloat     | Amount −100..100%; Automatic Center or manual Center                                                                          | Opposite anchor/control scaling; automatic center uses all anchor bounds; no Illustrator numeric parity claim                                                                                                                                                                                                                                                                                           |
| Wave Points        | Vertical / Horizontal; Amplitude ±100000 px; Wavelength 0.0001–100000 px; Phase ±360°; Keep Endpoints                         | Static sine displacement of existing anchors only; optional open-end pinning; relative Bézier handles unchanged; coordinate/wavelength ratio limited to ±1e12                                                                                                                                                                                                                                           |
| Taper              | Amount −50..50%; Horizontal / Vertical axis                                                                                   | Whole-input control bounds; scales the perpendicular dimension; nonlinear curve interior is approximate                                                                                                                                                                                                                                                                                                 |
| Bend Points        | Angle −180°..180°; Horizontal / Vertical axis                                                                                 | Bends all contours into a circular strip around the shared control-bounds midpoint; folds are rejected; nonlinear curve interior is approximate                                                                                                                                                                                                                                                         |
| Twist Points       | Angle −360°..360°                                                                                                             | Center-weighted radial angular warp in one shared whole-input bounds frame; positive is clockwise in screen coordinates; nonlinear curve interior is approximate                                                                                                                                                                                                                                        |
| Auto Handles       | Strength0–1                                                                                                                   | Preserves anchors; generates symmetric one-third-shorter-chord tangents;0 makes straight edges                                                                                                                                                                                                                                                                                                          |
| Instance on Points | Source Path; Points Path; Align Tangent (off by default)                                                                      | Places the whole-source bounds center at each authored target anchor. Off: translation only. On: source +X follows target handle B, reverse handle A, or a neighboring-anchor chord fallback. Curve interiors are not sampled; Resample Path upstream for even spacing or denser anchors. Target groups are ignored; copies are materialized, bounded geometry with separate groups and no global union |
| Halftone           | Source Fill or Image; Cell Pitch 1–1024 px; Invert Luminance; Grid Origin X/Y ±100000 px (default 0,0)                        | PaintedPath→PaintedPath; black, cell-clipped polygon dots whose area follows sampled darkness. Grid Origin is the top-left document-coordinate corner of cell (0,0); document Y increases downward                                                                                                                                                                                                      |
| Ordered Dither     | Source Fill or Image; Cell Pitch 1–1024 px; Invert Luminance; Grid Origin X/Y ±100000 px (default 0,0)                        | PaintedPath→PaintedPath; black, clipped cell squares selected by a fixed Bayer 8×8 threshold; ordered thresholding, not error diffusion; Grid Origin is the top-left document-coordinate corner of cell (0,0)                                                                                                                                                                                           |
| Line Screen        | Source Fill or Image; Cell Pitch 1–1024 px; Invert Luminance; Horizontal / Vertical; Grid Origin X/Y ±100000 px (default 0,0) | PaintedPath→PaintedPath; cell-sampled black stripes whose thickness follows darkness, clipped to the source mask or placed image rectangle; Grid Origin is the top-left document-coordinate corner of cell (0,0)                                                                                                                                                                                        |

All new numeric fields use the common drag/type interaction. Bypass is available for the
Path→Path effects. Common Session commands provide Undo/Redo and locks, and invalid edits
preserve authored state. Output/work budgets fail explicitly rather than freezing on huge input.
Derived geometry is marked unstable so downstream Path Remap edits suspend on changed input.
Direct canvas text-character editing through these effects is unavailable; edit upstream Text.
Halftone, Ordered Dither and Line Screen are paint-to-geometry effects: each accepts a filled,
ungrouped path made of closed contours with a solid or existing two-stop linear/radial gradient
fill, or an embedded Image. Open, grouped and stroke-only vector inputs fail explicitly. A source
stroke is not carried into the black output. Grid Origin shifts the grid phase while source geometry stays fixed; it is
the top-left document-coordinate corner of cell (0,0), and document Y increases downward. Cells
are sampled at their centers, then dots, squares or stripes are clipped to each cell and the
source's compound mask so holes stay empty. Gradient stops interpolate in the existing sRGB paint
space; relative luminance is calculated in linear light after compositing alpha over white. For
images, only noninterlaced 8-bit RGB/RGBA PNG assets are accepted by the luminance sampler. It
uses nearest-texel samples mapped through image placement and size, composites alpha over white in
linear light, clamps partial edge-cell samples to the closest image texel, and clips marks to the
placed image rectangle rather than its alpha contour. Image pixels stay runtime-only; the input is
limited to 4 megapixels and the same 4,096-cell work cap applies.
Halftone polygon area approximates darkness; ordered dither uses the fixed Bayer 8×8 threshold
matrix. Cubics are flattened at about 0.1 document unit. These vector approximations are not
print-calibrated. The bounded pipeline limits sampling to
4096 cells, generated/output geometry to 100,000 vertices and mask-mark intersection work to
2,000,000 vertex-pair units; curve-mask preflight has its own 32-contour / 256-authored-point /
512-flattened-vertex caps. Exhausted work budgets reject edits transactionally.

Line Screen adds a direct stripe mapping for the same sampled luminance field. Its default
horizontal operation emits a centered black rectangle across each full cell width; its thickness
is `darkness × pitch`. Vertical swaps those dimensions. Each cell is independent, so neighboring
strips join into stepped variable-width lines; white samples emit nothing and black samples fill
the whole cell. The configurable grid origin, cell-center sampling and compound-mask intersection
are shared with the other luminance modifiers. This is a cell-sampled vector texture, not continuous engraving or print-calibrated
screening. Its stripe motif is conceptually related to the line-screen spot function in Adobe's
[PostScript Language Reference, §7.4.4](https://www.adobe.com/jp/print/postscript/pdfs/PLRM.pdf#page=500),
which describes white pixels growing away from a line through the halftone-cell center; Nect's
rectangle construction does not implement PostScript/RIP screening, screen angles, device frequency,
or printed dot gain. Line Screen reuses the 4096-cell, 100,000-vertex, mask-intersection and
transactional output budgets. Its `horizontal` / `vertical` operation remains; the shared grid
origin uses schema 15.
Closed Path Offset's 32-contour / 256-authored-point input caps and 256 flattened / 512 output
vertex, 4096 subdivision-node, 300000 topology-check and 4096 estimated-join budgets are
aggregate across groups, not multiplied per group; its existing eight-nonzero-node graph cap
is unchanged. Any exhausted shared budget rejects the remaining groups, even if one might
produce empty erosion.

## Useful stacks

- Polygon/Star → Corner Round → Mirror (Keep Original) → Fill
- Curve/Text → Resample Path → Roughen → Smooth Path → Fill or Stroke
- Curve → Trim Path → Stroke. A Fill on an open path uses an implicit closing edge; omit Fill
  when you want only the retained curve segment.
- Curve → Subdivide Path → Remap → Fill for denser, editable anchors without changing the curve.
- Curve → Resample Path → Stroke → Expand Stroke → Boolean → Fill
- Curve → Dash Path → Stroke for actual separated path pieces.
- Curve → Set Path Start → Trim Path / Dash Path to move where closed-contour traversal begins while keeping its forward direction.
- Imported Path / generated Path → Set Closure → Stroke or Boolean to create a real closing segment using the retained endpoint handles.
- Curve → Reverse Path → Trim Path / Dash Path to reverse direction-sensitive start and pattern order.
- Imported polyline or Resample Path → Simplify Path for point reduction.
- Rectangle/Star → Pucker & Bloat → Fill for pointed or petal-like shapes.
- Line/Curve → Resample Path → Wave Points → Stroke for a static sine-like outline.
- Text/Path → Taper → Fill for size progression across a wordmark; Resample first for finer curve deformation.
- Line/Curve → Bend Points → Stroke or Fill for a circular strip; Resample first for finer deformation detail.
- Text/Path → Twist Points → Fill or Stroke for a center-weighted swirl; Resample first for finer deformation detail.
- Source Path → Instance on Points ← Points Path → Fill for repeated ornaments; enable Align Tangent to rotate each source +X along the authored target tangent. Use Resample Path before Points when even spacing is wanted.
- Linear / Radial Gradient Fill → Halftone for variable-size black dots; switch to Ordered Dither for a fixed Bayer texture.
- Halftone / Ordered Dither / Line Screen → move Grid Origin to phase-align the texture without moving its source. The [12-layer grid-origin playground](../examples/luminance-grid-origin.json) compares zero, quarter-cell, and half-cell offsets.
- Linear / Radial Gradient Fill → Line Screen for cell-sampled horizontal or vertical black stripes whose thickness tracks darkness.
- Existing Geometry Repeater, Closed Path Offset, Transform, Boolean and Join Geometry remain
  available; these additions do not replace them.

Open `examples/modifier-playground.json` in this development build for editable examples. It has
47 evaluated editable layers covering 18 modifier/effect families. To compare translation-only
and tangent-aligned Instance on Points, open the [11-layer tangent playground](../examples/instance-tangent-playground.json)
or rebuild it with `node scripts/instance-tangent-playground.js`. For luminance-to-geometry
examples, open the [12-layer luminance playground](../examples/luminance-playground.json), which
stores the evaluated Halftone and Ordered Dither outputs, or the [12-layer Line Screen playground](../examples/line-screen-playground.json), which compares horizontal and vertical mappings on the same gradient source. Rebuild the luminance playground with `node scripts/luminance-playground.js` or the Line Screen example with `node scripts/line-screen-playground.js`. Rebuild the main playground with
`node scripts/modifier-playground.js`; it validates all layers and exports SVG. Detailed evidence,
research sources and limitations: [editing refinement](editing-refinement.md).

For Count versus Maximum spacing, open [the two-layer density example](../examples/resample-spacing.json) in a schema-v12-capable build. Select each downstream Path Remap to inspect anchors: Count6 yields6/6 samples on120/240px contours; Maximum spacing24px yields6/11. Rebuild with `node scripts/resample-spacing-example.js`. Old Count documents load in this build; old binaries are not promised to read the new format.

- Path → Auto Handles → Stroke preserves anchor positions while generating smooth tangent handles. Strength0 clears handles; bypass restores the source.
- For the reverse direction, [the angularization example](../examples/angularize.json) compares the same curve with24/8/4 Resample points; source geometry remains editable. Rebuild with `node scripts/angularize-example.js`.
