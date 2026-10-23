# Image mask Luminance mode

This bounded extension adds luminance coverage to the existing Image/Keyer
alpha-mask pipeline.

The existing Mask gains Image mode: Alpha (default) / Luminance. A virtual
Boolean `luminance=false` leaves old JSON untouched. Actual authorship uses
schema52 and existing parameter/session/lock/source/history commands. One edit
is one Undo; implicit Alpha and repeated values are neutral. For closed Paths,
coverage remains geometric; the Image-only control is disabled for non-Image
material. No node, CPU evaluator, parser or general color-management change.

Contract: on the renderer-decoded, non-premultiplied sRGB R/G/B in [0,1],
Y = 0.2126 R + 0.7152 G + 0.0722 B. The new mask alpha is Y times the material's
original alpha. Alpha mode continues to use original alpha alone. This uses
explicit sRGB filter interpolation, consistent with the existing renderer;
there is no linear-light conversion. Gray128 therefore gives128/255 coverage,
not linear-light gray. White/black give1/0; half-white gives128/255 and
half-gray gives about64/255. Existing PNG decoding/color-profile behavior is retained.

The SVG pipeline uses an explicit feColorMatrix alpha row with those weights,
then feComposite against SourceAlpha once. Gaussian Feather applies once to the
resulting alpha; Invert complements it afterward. White flood/composite converts
coverage to the existing white luminance-mask representation. Image content and
paint stay outside that filter. Canvas/SVG/PNG share the same renderer and units.
Alpha-mode SVG and old Path SVG remain byte-identical. Existing finite sigma,
filter bounds, cycle/depth and asset guards remain.

References:

- [Filter Effects 1, feColorMatrix and color-interpolation-filters](https://www.w3.org/TR/filter-effects-1/#feColorMatrixElement)
- [CSS Masking 1, mask processing](https://www.w3.org/TR/css-masking-1/#MaskValues)

Acceptance: black/white/gray/primary colors, partial/zero alpha, source alpha
multiplication, Feather/Invert order/complement, placement, JSON/project reload,
old golden documents, Undo/no-op/locks/stale/invalid input/cycles. Add a small
editable Alpha/Luminance comparison. Focused model/DOM and production-Web
headless Canvas/SVG/PNG checks. Previous completed native checks are not rerun;
Luminance-specific native/physical/OS file-dialog/cold-reopen acceptance is separate.

The existing image-luminance-field dither sampler uses linear-light color
composited on white for dot density. That is a different operation from SVG mask
coverage (transparent material must give zero before Invert). It is not reused
or changed here; the new Image mode explicitly identifies sRGB in its option.

This bounded Image slice preserves existing geometric Path coverage, explicitly selects Alpha/Luma,
and fixes schema52's Image Luminance profile to 8-bit sRGB Canvas. SVG explicitly
declares sRGB; the UI names it. Tests verify the default Canvas context reports
sRGB/unorm8 and returns Uint8ClampedArray. There is no linear, ICC/color-managed
or AE-equivalence claim. A future profile requires explicit versioning.

Extra oracle: RGB[94,148,30] and RGB[162,108,226] both have exact integer weighted
numerator1,280,000, i.e. luminance128/255. With material alpha255/128 they yield
mask alpha128/64. Bright transparent white yields0 before Invert and1 afterward.
JSON/project roundtrip is distinct from actual OS save/reopen, which is not run.

Verification at product freeze: 51 focused model/DOM tests passed with no skips.
Nine production-Web/CSP headless documents passed exact Canvas-layer/SVG/PNG
RGBA equality and JSON reopen. The default Canvas context reported sRGB,
unorm8 and Uint8ClampedArray. Both equal-luminance colors yielded128 opaque
and64 at half alpha. Bright transparent white (Feather0) yielded0, then255
under Invert. All weighted-color oracle values matched exactly; Invert complement
error was zero. Partial source/material alpha was64 in Alpha and32 in Luminance.
The frontend build passed. Native compilation is a separate recorded stage.

21 prior JSON/SVG fixtures remained byte-identical; all183 files in the PR71,
PR72 and native-verification sealed artifacts matched their manifests. No prior
native check was rerun. See [the editable comparison](../examples/mask-luminance/README-ja.md).
