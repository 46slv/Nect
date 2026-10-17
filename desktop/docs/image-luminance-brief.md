# Embedded image inputs for luminance effects

**Status:** integrated as the bounded first image-input slice. Halftone, Ordered Dither and Line
Screen can consume the existing Image output; the other vector paint and geometry inputs remain
unchanged. See [modifier reference](modifiers.md) and [implementation notes](editing-refinement.md).

## Existing image and luminance paths

- An `Image` evaluates to `{kind: 'image', asset, position, size}`. `position` is its placed
  top-left document coordinate (after the existing position-reference adjustment); `size` is its
  positive display extent. `Image Output` keeps its independent Image-typed output. SVG embeds the
  original PNG.
- `src/raster.js` remains the DOM boundary: imported PNG/JPEG files are normalized to PNG by the
  browser before persistence. The synchronous, DOM-free evaluator is shared by browser preview,
  SVG export and `cli/export.js`.
- Image import keeps its existing file/project limits: 12 MB per file, 8,192 px per side and
  16 megapixels per asset; up to 100 image assets, 24 MB / 32 megapixels aggregate and 50 MB per
  project. These are not a guarantee that every valid PNG encoding is supported by image-luminance
  sampling.

## Socket and sampling contract

- The only new socket exception is an Image-typed output into the `path` input of Halftone, Ordered
  Dither or Line Screen. Fill, Stroke, Boolean, Transform, Path Remap and ordinary vector Output
  continue to reject Image. No generic Field or Image-to-Path conversion was added. `Image Output`
  remains independent and can forward its Image-typed descriptor to a luminance effect.
- Each effect reuses its existing cell pitch, grid origin, invert setting, mark generator and black
  `PaintedPath` output. The source is sampled at grid-cell centers in document coordinates (Y-down).
  Map a sample through the placed rectangle to intrinsic pixels with nearest-texel `floor`; no
  implicit filtering or resampling is performed.
- Only positive-area cells intersecting the image rectangle are generated. If an included partial
  edge cell has its center outside the placed rectangle, use the nearest edge texel; do not include
  wholly outside cells. Clip the resulting marks to the placed rectangle, including partial edge
  cells. Alpha is not a silhouette mask.
- Input RGB values are treated as sRGB, converted to linear light, then alpha-composited over white
  before the existing relative-luminance/darkness rule. Transparency therefore changes tone; a
  transparent black pixel is white paper in the non-inverted mapping. Invert remains the existing
  luminance mapping.
- Image bypass is disabled while its input is Image-typed: a same-type bypass would return Image
  data from a node that promises `PaintedPath`. Generated geometry can flow through later vector
  modifiers normally. Image pixels/RGBA are runtime-only; serialized projects store the original
  embedded PNG and graph/settings only.

## Bounded synchronous PNG adapter

- `src/image-luminance-field.js` lazily decodes only the asset referenced by a luminance node. It
  uses pinned Pako 3.0.2 (MIT; [source and license](https://github.com/nodeca/pako/tree/3.0.2)) via
  the browser ESM inflater with streaming output, an 8 KiB reusable output chunk and
  an IHDR-derived filtered-byte cap. Existing `pngSize()` checks the signature, chunk bounds/CRCs,
  dimensions and IEND before inflation; the adapter then validates supported PNG structure and
  output dimensions.
- The accepted pixel format is noninterlaced 8-bit RGB or RGBA. Required chunks are IHDR, one or
  more contiguous IDAT chunks, and final IEND. Optional metadata is limited to one 1-byte `sRGB`
  chunk (intent 0–3) and one 9-byte `pHYs` chunk, both before IDAT. Palette/grayscale, other bit
  depths, interlace, `PLTE`, `tRNS`, `gAMA`, `cHRM`, `iCCP`, `sBIT` and unknown chunks fail
  explicitly; no color-profile transform is applied. JPEG decoding is not added: imported JPEG
  images have already been normalized to PNG by the browser.
- A single runtime cache is keyed by exact immutable asset-object identity and retains at most 16 MB
  of RGBA. The initial processing cap is 4 megapixels per image and the existing 4,096-cell cap
  applies. Grid-count checks run before decoding. Unsupported/missing/corrupt data, dimension or
  decompression mismatches and geometry-budget failures reject evaluation atomically; no partial
  marks or fallback white samples are returned. One cold 4 MP Node 24 sample measured 301 ms; this
  is a bounded local measurement, not a cross-device latency guarantee.
- The Image evaluator cache now compares the actual asset object, in addition to existing identity
  and dimensions. A same-ID/same-size replacement object changes the Image output version and
  invalidates dependent luminance geometry. Clones reuse frozen asset references for history; cold
  JSON reopen creates ordinary fresh asset objects. The RGBA cache is never placed in a document or
  export result.
- Schema 17 marks the persisted Image→luminance input contract. Validation promotes a document with
  such an edge without adding a synthetic input field. Documents with only vector luminance sources
  keep their existing schema and default fingerprints.

## Verification target

The focused tests cover RGB/RGBA pixels, all PNG scanline filters, sRGB-versus-linear values,
alpha-over-white, placement/scaling, edge clamping, grid phase, output clipping, unsupported and
malformed PNGs, CRC/dimension/decompression/work limits, same-ID/same-size cache invalidation,
Undo/Redo, cold JSON reopen, and absence of serialized pixel buffers. Core/export checks compare
browser SVG and CLI SVG. The [Image luminance playground](../examples/image-luminance-playground.json)
compares the original image with all three effects. This slice does not claim arbitrary PNG support,
native-package acceptance or print/RIP calibration.
