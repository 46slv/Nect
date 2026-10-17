# Non-destructive vector modifier catalog

These nodes preserve the upstream source and can be reordered by graph wiring. Use Add node
search. Development build required: the previously distributed 7496aee trial cannot load these
new node names. Native Windows interaction and a replacement trial package remain open.

| Node          | Controls                                                             | Current contract                                                                                |
| ------------- | -------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------- |
| Mirror        | Left-right / Up-down / Angle; Axis Center; Axis Angle; Keep Original | Path before paint; live symmetric pair optional; no seam weld or raster reflection              |
| Corner Round  | Radius in px                                                         | Straight-edge corners only at nonzero radius; short edges clamp radius; open ends fixed         |
| Resample Path | Points per Contour, 2–1024                                           | Arc-length samples become polylines; closed paths need 3; low counts change details             |
| Roughen       | Amount in px per axis; Seed; Keep Endpoints                          | Deterministic position noise; no new points; handles translate with anchors                     |
| Smooth Path   | Factor 0–1; Iterations 1–50; Keep Endpoints                          | Neighbor-average positions; can shrink/collapse outlines; not curve fitting                     |
| Trim Path     | Start / End fractions 0–1                                            | Per-contour length; Start ≤ End; equal values empty; partial closed inputs become open, no wrap |
| Expand Stroke | Existing upstream Stroke width/cap/join                              | Solid straight-edge stroke → closed outline geometry; add Fill after; no type-preserving bypass |

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
- Curve → Resample Path → Stroke → Expand Stroke → Boolean → Fill
- Existing Geometry Repeater, Closed Path Offset, Transform, Boolean and Join Geometry remain
  available; these additions do not replace them.

Open `examples/modifier-playground.json` in this development build for editable examples.
Rebuild with `node scripts/modifier-playground.js`; it validates all layers and exports SVG.
Detailed evidence, research sources and limitations: [editing refinement](editing-refinement.md).
