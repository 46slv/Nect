# Dither shapes, size and spacing

For arbitrary motifs, prefer the separated
[Luminance to Points → Instance on Points composition](luminance-points.md).
The controls below remain shortcuts on the existing screening nodes.

Ordered Dither and Error Diffusion Dither retain their existing Bayer 8x8 and
Floyd-Steinberg algorithms. They now offer Square, Circle, Polygon and Star.
Halftone shares these shapes for continuous luminance-dependent dot sizing.
No new threshold, diffusion, smoothing or metaball algorithm is introduced.

| Control                     | Meaning and limits                                                                         |
| --------------------------- | ------------------------------------------------------------------------------------------ |
| Dot shape                   | Square, Circle, regular Polygon, or alternating-radius Star                                |
| Dot size                    | 1–1024 document px: square side, circle diameter, polygon/star outer circumcircle diameter |
| Dot spacing                 | Existing `pitch`, 1–1024 document px between grid centers; smaller means denser            |
| Polygon sides / star points | Integer 3–32; Star has twice that many vertices                                            |
| Dot rotation                | -360–360 degrees, clockwise in the document's downward-Y plane                             |
| Star inner radius ratio     | 0.05–0.95 of outer radius; 4 points and 0.18 make a narrow four-point star                 |

Size and spacing are independent authored Scalar parameters, with the usual
expressions, sockets, locks, CommandSession transactions and Undo/Redo. Changing
spacing does not rewrite size. Shape is the existing node operation selector.
Unused shape parameters remain saved when switching shape.

For either Dither, the existing algorithm first selects occupied cells, then
places a fixed-size mark at each selected center. Shape/size/rotation do not
change which cells are selected. For custom Halftone shapes every positive-
darkness cell receives a mark; each dimension is maximum size times
`sqrt(darkness)`, so its area is proportional to darkness for that shape.
Absolute coverage differs between shapes of the same diameter.

Halftone's default **Legacy calibrated** choice preserves its existing calibrated
circle-area screening, including cell clipping and the full-black square. Its
size remains controlled by spacing. Select a named custom shape to use independent
maximum size/rotation and to allow overlap. The Inspector explicitly explains
this compatibility mode.

## Overlap, sampling and alpha

Custom shapes and both Dithers are never clipped to their grid cells. They are
clipped only to the closed vector source mask (including holes) or the placed
image rectangle. Size can exceed spacing; a 20 px square rotated 45 degrees at
16 px spacing produces connected black areas with white diamond gaps. No new
Union command or smoothing is applied. The existing NonZero mask intersection
represents overlapping opaque black polygons as one silhouette, with unchanged
mask geometry budgets; overlaps do not darken by stacking alpha.

The sampling grid still covers source bounds only. Cells beyond those bounds do
not generate marks. Existing center sampling, nearest PNG texels, edge clamping,
sRGB-to-linear conversion, alpha compositing over white, Invert and Tone power
order stay unchanged. Fully transparent input is white before Invert; it does
not define an image contour mask. Original paint/image data and alpha are retained
without mutation; generated black geometry remains opaque as before.

Limits remain 4096 sampled cells, 100000 generated vertices and existing mask
work/output caps. Circle subdivision maintains <=0.1 px sagitta at maximum size.
High density, large circles or many star points can exceed these caps. The
existing error display rejects the entire transaction; Inspector guidance says
to reduce density or shape detail. No partial authored state is committed.

## Saved format and integration

Documents containing Halftone or either Dither promote to schema 36. Older apps
reject them rather than silently losing controls. Pre-36 files backfill size by
copying the whole pitch parameter (including its fallback, expression/socket
and lock) and default sides=4, rotation=0, inner ratio=0.25. Legacy ignored
Boolean operations become Square on Dither and Legacy calibrated on Halftone.
This preserves the legacy rendering; independent edits remain available after
load. A copied driver remains driven until explicitly disconnected/cleared.
Schema-36 documents must contain all four new parameters. Unrelated documents
do not promote. The schema constant lives in `src/dither-controls.js`; the
integrator must reconcile its number if another lane claims version 36.

Dedicated code/tests/docs are delivered separately from the small shared
`core.js`/`main.js` integration commit and schema expectation updates. Apply
both commits together. No native build, autosave, font or app-data changes are
part of this work.

## Verification and reference comparison

- `tests/dither-controls.test.js` verifies independent density/size, all shapes,
  four-point star radii, rotation, overlap with white holes, RGB/RGBA alpha order,
  persistence, individual Undo/Redo, locking and atomic direct/driven rejection.
- `tests/fixtures/dither-legacy-fingerprints.json` captures current baseline
  9e4cbf9 outputs for the four existing luminance example documents. New migration
  produces identical geometry/fingerprints for those documents.
- `node scripts/dither-controls-example.js` creates two small synthetic gradient
  fixtures in ignored `test-results/dither-controls/`. No user image is included.
  The exported star and rotated-square patterns were visually compared with both
  provided references. The latter joins black marks while keeping white diamond
  gaps, then transitions to isolated small dots.
- `node tests/dither-controls.e2e.js` exercises the production Web bundle under
  native CSP in an isolated headless browser. It checks actual Inspector fields,
  shape/sides/rotation/size/spacing edits, save/reopen, Undo/Redo, identical preview
  and SVG path data, and exact SVG-raster versus downloaded PNG pixel equality.
  Evidence: `browser-receipt.json`, `editor.png`, `comparison.png` in that directory.

Two old fingerprint assertions already fail on the unmodified baseline: the
pre-tone playground golden file and the pinned vector fingerprint in
`image-luminance.test.js`. Their actual baseline values equal this implementation's
values; those old assertions are left intact. The older image CLI test also uses
a URL pathname as Windows cwd, causing spawn ENOENT. Focused regression runs
exclude those three known cases; this is not a full-suite/native/physical-input
acceptance claim.

External-review gate: invoked for the saved-format/shape decision; consultation
not-required because current implementation, baseline comparisons, exact geometry
checks and production UI/export evidence settle the bounded choices. External
sends: 0; the parent remains the only authorized external consultation sender.

The shared source-mask input budget and exact zero-length font segment handling were repaired on 2026-10-20. See [dither-input-budget.md](dither-input-budget.md) for the finite preparation caps, unchanged clipping/output limits, evidence and native retest boundary.

## Image and Shape output retention / 2026-10-20

User requirement [REQ303](https://app.notion.com/p/3fdfd279a6f3815285a9fefa44391ab6): retain the Image-output form and the current Shape-output form. Neither representation is retired or replaced.

| Axis        | Current Desktop source                                                                                                                                                                                            |
| ----------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Input       | Existing Shape Dithers accept filled vector or supported Image input. New Ordered Dither Image and Error Diffusion Dither Image accept Image only.                                                                |
| Calculation | Ordered Dither uses Bayer 8x8; Error Diffusion Dither uses Floyd-Steinberg. These are algorithms, not output-format names.                                                                                        |
| Output      | Existing Dithers retain PaintedPath. The two new fixed Image types deliver derived dithered PNG data through Image Output and the shared Image consumer resolver. No output-type selector changes existing nodes. |

Source evidence: core.js outputType, socketCompatibleFor and the shared mask/mark evaluation branch. A read-only connection probe accepts Image -> either Dither and refuses either Dither -> Image Output with "Cannot connect PaintedPath to Image". PNG export of a Shape document does not constitute an Image-typed dither result.

The [Image-output contract](image-dither-output.md) records the exact user requirement separately from this slice's implementation choices: source pixel dimensions independent of placement, black marks on transparent support, preserved source-alpha sampling, finite raster/encoded/cache budgets, recipe invalidation and deterministic regeneration. Schema 41 saves the source and editable settings; derived pixels never enter authored imageAssets.

The earlier read-only reconciliation did not verify a historical Nect Image-producing Dither implementation. This addition makes no historical implementation claim. No further broad historical search is required.

Acceptance checks input type, calculation and output type independently. Source tests cover actual Image consumer pixels, preserved Shape fixtures, unchanged assets, deterministic reopen, Undo/Redo, bounded caches and atomic rejection. DOM integration covers add/connect/edit/output/history controls. Browser/native interaction remains a separate acceptance row until its actual run is recorded.

PR36's mask repair and existing Shape nodes remain preserved. The fixed Image route is an additional implementation; its verification and candidate provenance are recorded in the Image-output contract and current checkpoint.
