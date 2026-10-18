# Image Keyer and shape Mask (schema35)

Add **Keyer** after an Image. Set **Key color**, **Range** and **Softness**.
The key compares normalized Euclidean RGB distance in sRGB. Range removes
matching colors; Softness fades the next distance interval. The resulting
coverage multiplies the original alpha. Partial pixels keep straight RGB;
fully transparent RGB is zeroed for resampling. Key color alpha is ignored.
The original embedded PNG is retained. Bypass restores the input image.

Keyer accepts supported 8-bit, noninterlaced RGB/RGBA PNGs up to4 megapixels;
the existing image import limit remains16 megapixels. The document and an
effect chain allow at most8 active Keyers. Unsupported input returns an error.
There is no AI subject extraction, spill removal, video or dedicated image
texel eyedropper in this checkpoint.

Add **Mask** after an Image, Keyer or **Composition Reference**. Connect a
closed Path from Ellipse, Rectangle or another shape to **Shape**. Edit that
shape's position/size normally. **Invert** cuts out its interior. Source
paint, stroke order and alpha are retained. Open shapes are refused. The mask
uses the Path's nonzero winding rule and ignores its paint. Limits are256
contours and10000 points; feather is not included.

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
