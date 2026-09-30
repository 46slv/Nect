# R09-A2-LUMA-MASK-01 — first sRGB Luma Mask vertical

Status: Sol-frozen bounded implementation packet, 2026-10-01. Entry is clean synchronized `codex/practical-alpha@598d2d7c3825c3dafcf1e5ea5d607095a35188cb`, native writer 0.71, with A1 source-writer release. Authority: DEC-71 Mission, Completion Route R09/P09, the accepted G8 Alpha/Luma semantics baseline, and Confirmed mask Requirements. This packet does not close R09, L2, AE parity, or hands-on GUI acceptance.

## Scope and contract

- Admit `luma` as a third mode on the existing `Object::compositing.mask`; retain its stable mask/source IDs, enabled driver/expression, invert, Session/Undo, and A1 dependency checks. Do not create a second mask store or renderer.
- Declare a versioned `mask_color_space` value `srgb` in authored/native/API state. A2 accepts only that profile. Existing native 0.1–0.70 masks decode as Geometry/non-inverted/sRGB; native 0.71 masks decode as their saved Geometry/Alpha mode with sRGB default. The next writer version stores mode/invert/profile explicitly and rejects a falsely old version carrying new fields. Older readers must refuse its version. Profile changes later require an explicit versioned contract.
- Reuse A1's isolated evaluated source RGBA projection in Composition space, including source local effects, internal masks, children and opacity; ignore only source-root ordinary visibility/blend. The allowed source kinds remain Path, Text, Image and Group. Target order stays content → postchildren effects → mask → target opacity/blend.
- For each source pixel in the declared 8-bit sRGB profile, use non-premultiplied channel values `R,G,B` and source alpha `A`, each normalized to `[0,1]`: `m = (0.2125 R + 0.7154 G + 0.0721 B) * A`. A zero-alpha pixel has `m=0`; unpremultiply only when `A>0`. Apply invert afterward as `1-m`; outside source coverage has `m=0`. Convert the resulting mask value to the existing 8-bit premultiplied coverage surface with explicit deterministic rounding. These coefficients are the SVG `luminanceToAlpha` matrix cited by [W3C CSS Masking](https://www.w3.org/TR/css-masking-1/#MaskProcessing) and [SVG 1.1 filters](https://www.w3.org/TR/SVG11/filters.html#feColorMatrixElement).
- Native, Session/API, MCP and desktop Inspector expose the same mode, invert, source, enabled state and sRGB profile. SVG and any derivative without an exact projection refuse or loss-report enabled Luma explicitly; disabled masks may export matching visible output.

## Acceptance

- Independent transparent-background pixel oracle: opaque red `#ff0000` and gray `#363636` both yield 54/255 mask alpha under the sRGB formula despite different RGB; opaque green yields 182/255, opaque blue 18/255, transparent white 0/255, and 50%-alpha white yields 128/255 within the current 8-bit projection rounding. Inspect source projection bytes and final target alpha separately if premultiplication creates a one-unit difference.
- Invert yields the complement, including outside source coverage within target bounds. Source opacity, transform, Group children/effects/internal masks, hidden root and descendant visibility are covered. A1 Geometry/Alpha pixel oracles remain unchanged.
- Set/Undo/Redo, stale revision, native encode/decode/Save As/cold reopen, API/MCP and desktop control/readback preserve the exact profile and mask identity. A 0.71 Alpha file opens without drift to its appearance and becomes explicit `srgb` on a new save.
- Missing/cross-Composition/self/indirect-cycle source, nonfinite/singular transform, invalid mode/profile, dangling deletion, malformed fields, resource/depth failure and unsupported derivative refuse without partial Document/history/native/source mutation. Full Release build and serial regression pass before checkpoint closure.

## Residuals

No linearRGB or ICC-managed profile, 16-bit/high-dynamic-range processing, AE Luma oracle, feather/expand/combine, or generic derivative bake is claimed. Actual hands-on GUI acceptance remains `LOCAL_WAIT` until observed on the product host; offscreen widget/pixel tests are separate evidence.
