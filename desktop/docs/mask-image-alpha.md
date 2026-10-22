# Image alpha as Mask input

Parent-selected production slice from PR71 HEAD
`8d657b744a1b5259eab16ab0ed19fbda920ac865`. The existing Mask material socket
accepts only a closed Path. Extend that same socket to Image-typed outputs,
including Image and Keyer, without a new node, evaluator or luminance mode.
Current worktree is clean at the verified remote base; the ownership check found
no other active Nect source writer. This lane owns core/main/appearance-mask.

Keep the persisted Path socket representation for old documents. Only an actual
Image connection requires schema51; reading or editing a Path mask does not
upgrade it. The source picker and wire compatibility share the same rule. Label
the material input `Shape / image alpha` in the existing Mask fields.

Render the input Image using the existing embedded-asset/effect renderer and its
document-coordinate position/size. Extract SourceAlpha, optionally apply one
Gaussian blur with the existing sigma, then optionally complement that alpha.
Flood/composite white into the existing luminance mask so source RGB cannot
affect coverage. A later appearance Transform moves/scales the masked source
and material together. Arbitrary pre-Mask Image Transform routes are not added;
the current Image graph supports its position and size. Existing Path mask and
feather SVG output remain unchanged. Source paint, alpha, stroke and IDs remain.

Keep the existing cycle/lock/session/asset guards. Missing inputs remain an
ordinary incomplete wiring state. Rejected type/cycle/invalid data edits are
atomic, with one successful connection/edit per Undo and neutral no-ops. Image
filters use the existing finite region budget even at sigma0. No new decoding
or pixel evaluator is introduced. PNG is exported from the same SVG.

Acceptance uses small embedded synthetic images: alpha0/128/255, different RGB
with identical alpha, Keyer alpha, placement/size and post-Mask transforms,
Feather/Invert complement, JSON/project roundtrip, Undo/no-op and Path golden
regression. Provide an editable JSON/SVG/PNG example and focused production-Web
headless checks. No new native GUI, OS save dialog, main merge, release or upload.
PID139120 was absent in the exact-PID read-only query; no termination or profile
operation was performed. Preserve all earlier candidates; build/TEMP/output on D
where supported, with the existing fixed frontendDist dependency on C.

Verification: 37 focused tests passed with no skips (8 new model tests, existing
Mask/Feather model/DOM, Image effects and Composition Reference). PR71's nine
saved JSON/SVG documents remained byte-identical under the new code. Production
Web/CSP headless checks passed for twelve synthetic documents, including actual
Web Export PNG downloads and full RGBA equality against the corresponding SVG.
Normal/Invert complement error was zero over both hard and feathered fixtures.
RGB-only material changes produced identical output RGBA. Partial source alpha
128 times material alpha128 produced alpha64; referenced stroke alpha survived.
The Graph input picker performed Image/Keyer switching and one Undo/Redo, and
same-connection edits stayed neutral. No assertion was skipped or tolerance
expanded; plateau samples avoid bitmap interpolation bands, and a feathered
opaque region is compared against the same blurred material (not unblurred255).

The [editable example](../examples/image-alpha-mask/README-ja.md) contains Image
and Keyer+Feather documents with their SVGs. Only Graph positions were adjusted
from the verified fixtures; exported SVG remained exact. PNGs/evidence stay on D.
The 28 prior EXEs and 423 sealed files matched before the build. Native GUI,
physical input, OS dialogs and normal OS cold reopen are not verified here.

Reproduce focused checks with `node --test tests/mask-image-alpha.test.js
tests/mask-feather.test.js tests/mask-feather-dom.test.js tests/image-effects.test.js
tests/composition-reference.test.js`. After the frontend build, run
`node scripts/verify-mask-image-alpha.mjs <new-absolute-output-directory>` with
process TEMP/TMP on D. The headless script blocks external requests and refuses
an existing output directory.
