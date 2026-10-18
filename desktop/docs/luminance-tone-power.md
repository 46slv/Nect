# Luminance tone power

Halftone, Ordered Dither, Line Screen, Error Diffusion Dither and Luminance Instances share **Tone power · darkness response**. It is an artistic power curve with a finite Scalar range **0.1–10**, default **1**. Values below 1 increase midtone darkness; values above 1 reduce it. Zero/one darkness endpoints remain zero/one.

## Exact order and boundaries

1. Keep existing paint sampling (sRGB stop interpolation) or PNG nearest-texel sampling unchanged.
2. Convert to linear light, composite alpha against white, and calculate relative luminance using the existing sampler.
3. Form darkness as `1 - luminance`, or `luminance` when Invert is enabled.
4. Raise that darkness to `tonePower`. Power 1 returns the original darkness directly, without a new arithmetic operation.
5. Feed remapped darkness to the existing dot-area, strip-width, Bayer threshold, Floyd–Steinberg diffusion, or motif scale/angle mapping. Diffusion happens after the power curve. Mask clipping and motif-center selection are unchanged.

The luminance value, original paint/image, source rendering and color space do not change. Fully transparent samples still composite to white; Invert therefore makes them black before tone response. This is not display gamma correction or print calibration. There are no new levels/contrast endpoints, random inputs or field types. Existing cell, decode, geometry, work and instance limits are unchanged.

The numeric Inspector supports direct values, existing Scalar expressions/connections and parameter locks. Changes use normal command transactions, Undo/Redo and JSON serialization. Invalid fallback or driven power values reject atomically, including on empty source paths. Locking the parameter prevents editing it as with the existing controls.

## File compatibility

Documents with any of the five consumers promote to schema 21. Pre-21 documents backfill missing power with 1, using the incoming file version for every node before promotion. This remains independent of node/layer order and coexists with earlier grid-origin migrations. Schema-21 files missing the required parameter reject. Documents without these consumers are not promoted for tone power.

Default power is omitted from output fingerprint additions, and both samplers retain their original return shape. Exact pre-change geometry and fingerprints were compared on all four existing luminance playground files; pinned legacy fingerprints remain in the focused test fixture. Older Nect versions reject schema 21 instead of silently discarding the control.

## Editable comparison and verification

- [Editable native JSON](../examples/luminance-tone-power.json): 36 layers, 15 comparisons of all five consumers at power 0.5, 1 and 2, identical gradient and grid phase.
- [Actual exported SVG](../artifacts/luminance-tone-power.svg) and [Inkscape-rendered PNG](../artifacts/luminance-tone-power.png).
- `node scripts/luminance-tone-example.js` regenerates the JSON/SVG and independently cold-reopens the saved document in a fresh process, requiring byte-identical serialized geometry/fingerprints and SVG.
- `tests/luminance-tone.test.js`: identity, endpoints, invert/alpha order, finite range, all-five changed output, lock/history, invalid direct/socket/expression rollback, migration order and pinned default fingerprints.
- `tests/image-luminance-field.test.js`: real transparent/half-transparent/opaque PNG samples, inversion and power endpoints.
- `tests/luminance-modifiers-dom.test.js`: actual Inspector bounds, default and value editing through the editor.

Verification is focused Node/DOM regression tests, one Web build, fresh-process reopen and visual inspection of the actual SVG rasterization. No native package, browser session or full-suite pass is claimed.
