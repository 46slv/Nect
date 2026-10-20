# Image-output Dither

Requirement: [REQ303](https://app.notion.com/p/3fdfd279a6f3815285a9fefa44391ab6).

## User requirement

Retain both the Image-output representation and the current Shape-output
representation of dithering, and record the detailed settings in the specification.
Existing Shape Dither nodes, saved documents and connections must keep their
current behavior. The Image representation must deliver actual dithered image
data to Image consumers. The source artwork and node settings remain editable.

The user has not prescribed every algorithm/output combination, node naming,
pixel resolution, output alpha, or cache implementation. The following are bounded
implementation choices for this first Image-input slice, not claims about a
verified historical Image-output implementation.

## Implementation contract

- Add **Ordered Dither Image** and **Error Diffusion Dither Image**. Each has a
  fixed Image input and Image output. Existing Shape types keep PaintedPath output.
  The node operation selects the existing Square/Circle/Polygon/Star mark shape;
  it never changes output type.
- New node types require schema 41; older documents retain their existing version
  and output contracts. Older readers reject schema 41 instead of dropping nodes.
- Reuse the existing image luminance sampler and Bayer 8x8/Floyd-Steinberg mark
  selection, including pitch, grid origin, Invert, Tone power and independent mark
  size/shape settings. This slice accepts Image input only.
- Output pixel width and height equal the resolved input PNG dimensions. The Image
  value retains the input's document-space position and placement size. Moving or
  resizing placement can change grid phase/sampling without changing pixel count.
- The initial representation is black marks on transparent support, clipped to
  the placed Image rectangle, as in current Shape Dither. Generated mark coverage
  is opaque black inside and transparent outside; overlap is a union, never alpha
  stacking. A dedicated 4x4 coverage scan of the existing generated polygons adds
  deterministic edge antialiasing. This is not a general vector raster engine.
- Preserve the existing source-alpha semantics: nearest source texels are converted
  from sRGB to linear light and composited over white for luminance, then Invert
  and Tone power apply. Fully transparent source is white before Invert. Original
  RGB/alpha are never overwritten. Output alpha is mark coverage, not a copy or
  multiplier of source alpha, and sampling over white does not whiten output alpha.
  Output PNG channels are straight sRGB RGBA8, black RGB even at zero alpha.
- Evaluated Images hold only a bounded recipe on the existing source reference.
  The shared Image resolver generates the encoded PNG for preview, SVG, PNG export,
  Keyer and luminance consumers. Evaluation never adds or replaces imageAssets.
  JSON saves source plus authored settings; reopening regenerates the same pixels.

## Finite budgets and invalidation

- Existing source processing limits apply: supported noninterlaced RGB/RGBA8 PNG,
  at most 4,000,000 pixels, existing 8192-pixel side limit and 12,000,000 encoded
  bytes. Output has the same pixel dimensions and a 12,000,000-byte PNG ceiling.
- Keep existing 4096 sampled cells and 100,000 generated vertices. Before allocating
  output RGBA, reject raster preparation exceeding 8,000,000 edge/subrow samples.
  Output RGBA is at most 16,000,000 bytes. Raster coverage uses one output-width
  row rather than a supersized image. Polygon/edge storage is bounded by the
  existing generated-vertex cap. These are component bounds, not a process-heap
  measurement.
- At most eight Image-effect recipes per value and eight active Image Dither nodes
  per document. Existing eight-Keyer admission remains. The shared resolver retains
  only one compressed derived result, bounded by the encoded ceiling, plus at most
  eight recipe keys. The existing decoder retains one RGBA image (16,000,000 bytes).
  No pixel or PNG data enters per-node evaluation-cache values or history snapshots.
- Invalidate on immutable source-asset identity/data, ordered recipe settings,
  placement, and ordinary graph dependency versions. Cache eviction/recomputation
  must preserve deterministic bytes and may not alter authored state.
- Preserve the existing 50 MB serialized-project ceiling. Budget and malformed
  input failures reject the whole command with document/revision/history unchanged.

## Verification boundary

Required evidence: real Image socket connection and resolver pixels; unchanged
source/assets; stable old Shape fixtures; save/reopen, deterministic regeneration,
Undo/Redo and atomic limit rejection; bounded caches without authored asset growth;
focused source and DOM/web checks; separate candidate build. Browser/native UI
smoke waits for explicit release of the other lane's WS UI ownership.

## Recorded verification

- Focused source/DOM regression: **117/117 PASS** across Image Dither, old Dither
  controls/input budgets, Images, Keyer/Mask, image luminance, node catalog, named
  presets, Bypass and common command Session/menu cases. Dedicated pre-existing
  Luminance Instances motif-bypass probe: **1/1 PASS**.
- Existing Shape fixture outputs/fingerprints remain exact. New pixel tests check
  both algorithms, linear-light midgray, source-alpha sampling, transparent support,
  rotated/overlapping marks against an independent winding oracle over the preserved
  Shape path, Keyer/luminance/viewer consumers, source replacement, cache eviction,
  deterministic reopen, presets, Undo/Redo and atomic cell/vertex/raster/node limits.
- DOM integration runs the actual web editor module in happy-dom without launching
  a browser. Both Image nodes appear in the picker; typed source/output wiring,
  mark controls, history and SVG Image rendering pass.
- Production web build: **PASS**, 138 modules. Native build configuration, scoped
  Prettier, main/core syntax and `git diff --check`: **PASS**.
- Separate native release-profile build with `custom-protocol` and locked Rust
  dependencies: **PASS**. Candidate EXE is 9,995,776 bytes, SHA-256
  `9c625fc2bc2a8592b2410921b642c1e45829aa728bf6899d079094f8a5943d3b`.
  Its provenance is recorded in `image-dither-build.json`; it has not been launched.
- Production browser/CSP interaction and native UI smoke: **NOT_RUN**, pending
  explicit release of the other lane's exclusive WS UI ownership. Full suite:
  **NOT_RUN**. No browser/native visual, file-dialog or physical-input acceptance
  is claimed by source/DOM tests or by compiling the executable.

The extra whole-file Luminance Instances probe exposes a pre-existing schema
expectation `21` versus actual `24`. The same selected test fails on an extracted
3922a425 source snapshot. That unrelated assertion remains unchanged. The Bypass
catalog test's outdated unsupported list also fails on 3922a425 at Keyer; this lane
corrects the classification for existing Image/Mask/motif identities and adds the
new Image Dither identities, while their dedicated functional tests remain active.
Local receipts are in ignored `test-results/image-dither/`.

Editable synthetic comparison: [Image and Shape Dither](../examples/image-dither-output.json).
Top row is Image, bottom row is the preserved Shape representation; Ordered is on
the left and Error Diffusion on the right. `node scripts/create-image-dither-example.mjs`
regenerates the example without user images.

Status: source implementation, focused verification and separate candidate compile
complete; UI acceptance is the next checkpoint after its ownership gate is released.
