# Image Keyer and shape Mask

Add **Keyer** after an Image. Set **Key color**, **Range** and **Softness**.
The key compares normalized Euclidean RGB distance in sRGB. Range removes
matching colors; Softness fades the next distance interval. The resulting
coverage multiplies the original alpha. Partial pixels keep straight RGB;
fully transparent RGB is zeroed for resampling. Key color alpha is ignored.
The original embedded PNG is retained. Bypass restores the input image.

Keyer accepts supported 8-bit, noninterlaced RGB/RGBA PNGs up to4 megapixels;
the existing image import limit remains16 megapixels. The document and an
effect chain allow at most8 active Keyers. Unsupported input returns an error.
Key color's **Pick from input** opens a temporary preview of the input immediately
before this Keyer. It picks a visible texel's straight RGB, including earlier
image effects; transparent and outside pixels are not adopted. Driven fields use
**Pick base from input**. See the [editable production example](keyer-mask-example.md).
AI subject extraction, spill removal and video are not included.

Add **Mask** after an Image, Keyer or **Composition Reference**. Connect a
closed Path or an Image/Keyer output to **Shape / image**. Path inputs
use nonzero winding and ignore paint; Image inputs use alpha only, ignoring
RGB. Image position/size use the same document coordinates as the appearance.
**Invert** complements coverage. Source paint, stroke order and alpha are
retained. Open Path shapes are refused. Path limits are256
contours and10000 points. **Feather σ (document units)** softens only the mask
alpha, with Gaussian standard deviation from 0 to 256 at the Mask input.
Zero keeps the original sharp Path mask and SVG; Image alpha still uses its
alpha-extraction filter, without Gaussian blur. Invert complements the same
softened alpha. Later appearance transforms scale the source and mask together.
See [Mask feather](mask-feather.md) for bounds, transforms and schema50.
Image connections use schema51; old Path documents are not upgraded. See
[Image alpha Mask](mask-image-alpha.md) and the
[editable example](../examples/image-alpha-mask/README-ja.md).

Mask returns **PaintedPath**. Click its **Use as output** to connect it. In an
Image Output composition, this explicit action changes that Output to regular
Output, preserving its ID and custom name. Undo restores its old type and
connection in one step. Locked outputs/inputs are protected. Creating a Mask
alone never changes the Output. Image-only consumers must come before Mask.
Converting a masked appearance to geometry is refused rather than losing it.

Assign an Image or Mask result to **Viewer1/2** to inspect transparency on a
checkerboard. **Alpha** shows coverage as gray, with sRGB128 for alpha128/255.
This temporary Viewer setting does not affect the document, Undo or exports.
SVG and transparent PNG use the same common renderer. Source and mask export
restrictions are preserved through Composition references and transforms.

Verification: `node --test tests/image-effects.test.js` and
`npm run build && node tests/image-effects.e2e.js`. The latter uses the actual
production bundle/CSP and browser controls, checking alpha pixels, PNG export,
Bypass, inversion, Undo and exact SVG after JSON reopen. Native Windows chooser,
manual IME and high-DPI interaction remain separate acceptance items.

**Image mode** defaults to **Alpha**. **Luminance (sRGB)** instead uses the
material's renderer-decoded sRGB brightness times its alpha, then Feather and
Invert. Closed Path masks retain geometric coverage. See
[the mode contract](mask-luminance.md) and the
[editable comparison](../examples/mask-luminance/README-ja.md).
