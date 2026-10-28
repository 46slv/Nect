# Auto Chroma proposal foundation

`proposeAutoChroma` is a pure proposal stage, not a document mutation or a complete
palette UI. It accepts stable slot IDs, explicit lightness/hue/role coordinates,
optional protected colors/manual chroma, and an optional verified coverage result.
It returns proposed literal colors without modifying slots or creating bindings.

The role profiles and bounded area compensation follow the accepted Color Guard
v1.1 prototype. Area is a soft input to chroma, not a target composition quota.
Protected colors retain their exact HEX and alpha; generated colors preserve an
explicit slot alpha. Zero colorfulness produces neutral colors. Manual chroma is
limited to a valid sRGB interval and reports when that limit changed the request.

The original prototype's first-crossing chroma bisection is not sufficient near
the blue cusp: the valid radial slice can be disconnected. The implementation
splits the cubic linear-RGB channel functions at their extrema and cube-boundary
roots, then finds valid intervals. It preserves valid blue overrides and never
selects an out-of-gamut gap merely by multiplying the outer chroma maximum.

Unit validation includes independent forward color-matrix checks, primary-color
round trips, blue-gap regression, protected anchors, identity/purity, bounded
area weighting, invalid input, and variable palette sizes through 256 slots.

The typed `set-color-slots` command applies up to 256 validated updates as one
Session transaction. Targets and values are preflighted together; no-op edits
retain Redo, stale revisions fail closed, and one Undo restores the entire batch.
This is an application primitive, not automatic adoption of generated proposals.

Still required: persisted recipe and dependency design, palette controls,
contrast/semantic guards, browser and native acceptance. These heuristics cannot guarantee aesthetically correct colors.

## Transient saturation preview

Shared colors now offers profile and colorfulness controls, per-slot Keep locks,
and before/after swatches. Preview and Cancel do not write the document. Apply
uses one `set-color-slots` command, so the full palette is one Undo/Redo. Preview
is invalidated by session/revision or literal changes and cleared on panel close.
Only applied literal colors persist; these preview controls are not saved recipes.

Current literal HEX is converted to OKLCH; lightness/hue/alpha are retained within
8-bit conversion precision. Achromatic colors keep their original literal rather
than acquiring an arbitrary hue. Dark and near-white slots receive Text and
Background weights respectively, with Primary for other slots. Those are temporary
heuristics, not semantic roles authored by the user. Area compensation uses a deep-copied current coverage snapshot when available.
The checkbox can disable it. A missing, pending, failed or stale measurement is
explicitly labeled unweighted; it is never substituted with older coverage.
A preview that used coverage cannot be applied after its scene/font key changes.
Contrast and semantic guards remain unimplemented and the UI states that limit.
