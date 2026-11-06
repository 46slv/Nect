# Image Sample Luminance slice

This internal, unconnected evaluation slice accepts explicit RGBA8 source
identity and revision, a Sample-frame-to-Image-Pixel affine, and a center plus
two full basis vectors for every Sample. It does not define saved Field syntax,
generic Space literals, a product producer, or a default edge/filter policy.

The supported request is Cell Average with Transparent edges and compositing
over opaque white. In this slice the footprint is the rectangle bounded by
`center ± u/2 ± v/2`. The transformed basis must be nondegenerate and
axis-aligned; rotation or shear is rejected instead of approximated. A Sample's
full footprint remains its denominator even outside the source. Contributions
are integrated in linear-sRGB premultiplied by alpha before white compositing;
luminance uses the adopted coefficients and darkness is `1 - clamp(Y, 0, 1)`.

The caller supplies the sample and pixel-visit budgets. Internal defensive
ceilings are 65,536 Samples, 16 million pixel visits, and the existing image
decoder's four-million-pixel source limit; these are not product defaults.
Missing capabilities, frame/source mismatches, unsupported modes, over-budget
work, invalid byte buffers and unrepresentable footprints reject. A fixed
non-shared input buffer is copied after validation; no result or cache is
published on failure. No implicit re-sampling or Space conversion occurs.

The legacy Luminance node remains unchanged. This slice is not yet connected
to a production node, project serialization, Image Color Field, or the complete
Grid/Luminance/Instance user workflow.
