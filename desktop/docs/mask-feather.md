# Mask feather

Parent-selected production slice from PR70
`bbd814056afa677dfdf2fecc32a62707c845dc38`. Add Feather sigma to the existing
Mask field list; no new evaluator, panel, image blur or luminance-mask feature.

Feather is Gaussian standard deviation, in document/artboard units at the Mask
input. It is not a width or CSS screen px. Shape transforms before Mask change
the path, not sigma. The existing appearance Transform after Mask transforms
source and softened mask together; nonuniform scale also scales sigma by axis.
This follows the current painted-group renderer, with no double transform.

Finite values from 0 to 256 are supported. Positive filters are limited to 8192
units per region side and 16 million square document units, with 4-sigma padding beyond the old
source/stroke bounds padding. This finite SVG filter region bounds work and
places the Gaussian tail outside visible source paint. Exceeding limits rejects
the command. Bounds limits apply only when feather is positive.

Blur SourceAlpha of the closed mask path only, then optionally apply 1-alpha
for Invert. White flood/composite converts that alpha to the existing luminance
mask representation. Image/paint/stroke/opacity remain unfiltered. SVG and PNG
share the renderer. SVG geometry precision does not round sigma to zero.

Zero keeps the exact old mask/SVG path with no filter. Missing feather is a
virtual Scalar 0; validation/load/serialize/UI reads do not add it to old JSON.
First no-op 0 edit is neutral. Actual parameter authorship uses schema50 and
existing Scalar/base/expression/reference/lock/history commands. Undo restores
the previous schema/data; no automatic bake or migration of PR70's samples.

Acceptance: strict old JSON/SVG golden equality, zero/no-op/Undo/Redo, invalid
and resolved-driven values/region budgets/locks/stale rejection, JSON/project
reload, alpha transition, unchanged internal color/detail and source alpha,
Invert complement within one 8-bit rounding unit, stroke bounds, and transforms
before/after Mask. Focused model/DOM and headless product-Web SVG/PNG checks.
No interaction with pending PID139120/profile, no new native GUI/OS dialogs.
Build/output/TEMP use D where supported; fixed frontendDist still needs a small
C reserve. No main merge, release, installer or Library upload.

Verification: 44 focused tests passed across feather model/DOM, image effects,
Composition Reference, numeric scrubbing and Color value expressions. The
production Web bundle/CSP passed 9 headless document cases: JSON/SVG reopen
equality and full RGBA equality of exported PNG versus that SVG. The sampled
soft edge rises from alpha 7 at x=12 to 255 at x=32 (sigma 4, edge x=20);
the full-image Invert complement had zero 8-bit error. Image color/detail,
source partial alpha and referenced stroke were preserved. Pre-Mask shape
rotation/nonuniform scale and post-Mask rotation/nonuniform scale were checked.

Run `node --test tests/mask-feather.test.js tests/mask-feather-dom.test.js
tests/image-effects.test.js tests/composition-reference.test.js
tests/numeric-scrub-dom.test.js tests/color-value-expressions.test.js`.
After the frontend build, `node scripts/verify-mask-feather.mjs <new-output-dir>`
runs the headless checks. Use a fresh absolute directory and process TEMP/TMP
on D. Evidence and PNGs are outside Git. Native GUI, physical input, OS dialogs
and normal OS cold reopen remain unverified; the pending test instance/profile
was not used.
