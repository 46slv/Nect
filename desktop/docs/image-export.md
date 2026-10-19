# SVG, PNG and local images

Editable JSON remains the source of truth. SVG and PNG are evaluated artwork exports, not project files. Nothing in this workflow uploads the artwork or fetches arbitrary image URLs.

## Export

- SVG uses the entire artboard (`width`, `height`, zero-origin `viewBox`) with no background. The SVG export units selector defaults to `px`; choosing `mm` or `cm` writes explicit physical root `width`/`height` values while `viewBox` stays in canonical document px. Artboard display units do not change export units. Text is outlined. Visible artwork layers are ordered as in the editor; hidden layers and guide layers are omitted. Selection, editing handles, grid and guides never enter the export scene.
- PNG rasterizes that same generated SVG into a transparent canvas. The toolbar offers the existing 0.5×, 1×, 2× and 4× scale choices and a physical-size × PPI mode. PPI mode calculates each output side as `round(document px × PPI / 96)`, using Nect's 96 CSS-pixel-per-inch document reference; the live preview shows final pixel dimensions and disables Export PNG when they exceed the supported range. Scale mode remains the default. Dimensions round to whole pixels. The browser canvas encoder writes 96 dpi metadata where supported; the chosen PPI controls pixel dimensions only and does not replace that metadata. SVG exports are bounded to 50 MB including repeated embedded images. PNG output is bounded to 8192 pixels per side and 16 megapixels; choose a lower scale or PPI for a larger artboard. The same validated dimensions are passed to the PNG rasterizer, which scales the source SVG viewBox once to that output size.
- Evaluation errors abort export rather than silently dropping visible artwork. Native saves stage beside the destination before replacing it; PNG uses a bounded binary save command. Cancel leaves the document unchanged. Repeated clicks during an export do not open competing saves.

## Import and editing

Import image adds a new composition with an Image generator and Image Output. PNG and ordinary baseline/progressive JPEG are supported. WebP, GIF, APNG and SVG image input are deliberately not included in this first bounded adapter. SVG still has its separate conservative vector import.

Local images are header/checksum-checked, decoded, and normalized into PNG. This keeps browser-decoded orientation/alpha while dropping metadata and file references. Limits are 12 MB per input/embedded PNG, 8192 pixels per side, 16 megapixels per image, 24 MB of embedded PNG bytes and 32 megapixels across assets. JSON remains bounded to 50 MB including fonts and images. High-entropy JPEGs can be rejected if their normalized PNG exceeds the limit.

Image position is top-left; size is width/height. Initial placement is centered and fitted down to the artboard without upscaling. Select the Image node to move it on canvas, or edit its position/size fields. Size is independently editable, so changing aspect ratio intentionally stretches the image. Layer visibility, guide status, locking, order, Undo/Redo and JSON reload are preserved. Fill color belongs to vector paths and does not tint an image.

Image output has its own socket type. Boolean, Convert to Path, Path Remap, Text to Path and Transform continue to require Path; a raster is never implicitly traced or treated as path vertices. Pen drawing needs a vector composition. Image transforms currently use its position/size fields. Effects, raster rotation, clipping and tracing are outside this increment.

Import cancellation, decode errors, oversized files and a document changed while reading preserve the current document. Browser/native JSON open preflights embedded-image decoding before replacing the document. The DOM-free core validates the PNG structure/dimensions but does not itself implement a PNG pixel decoder.

## Roundtrip boundary and future integration

A vector-only exported SVG can be reimported when it stays inside the documented supported subset. An exported SVG containing an embedded `<image>` renders in ordinary SVG viewers and in PNG export, but this editor's conservative SVG importer **explicitly rejects it**. Reopen the saved JSON to continue editing embedded images. Unsupported SVG attributes/elements are errors, never silent approximations.

The schema-v1 extension is optional `imageAssets` plus Image/Image Output nodes; existing vector-only JSON is unchanged. Core `exportScene` and `renderElement` share the rendering representation; `images.js` owns portable asset validation; `raster.js` owns browser decoding/rasterization; `storage.js` owns browser/native delivery. Future paint/stroke/gradient/clip work can extend these boundaries without mixing UI decorations with render state. A future Nect integration remains a separate decision.

## Verification boundary

`tests/images.test.js` covers real file SVG export/reopen, embedded JSON/headless SVG, type safety, limits, scene exclusion and history. `tests/images.e2e.js` covers actual file downloads/imports, PNG dimensions and alpha/color pixels, PNG/JPEG decoding, move/Undo/Redo, guide/hidden exclusion and interrupted/error/stale flows. Synthetic tests alone are not pixel evidence. Windows Chrome CI and native WebView2 are distinct: native WebView2 dialog interaction remains unverified until exercised on a connected Windows computer.
