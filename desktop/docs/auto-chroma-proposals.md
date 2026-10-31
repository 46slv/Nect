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

The harmony wheel and version-2 recipe are described in [harmony-wheel-core.md](harmony-wheel-core.md).
Still required: contrast/semantic guards and full-system browser/native acceptance. These heuristics cannot guarantee aesthetically correct colors.

## Transient saturation preview

Shared colors now offers profile and colorfulness controls, per-slot Keep locks,
and before/after swatches. Preview and Cancel do not write the document. Apply
uses one `set-color-slots` command, so the full palette is one Undo/Redo. Preview
is invalidated by session/revision or literal changes and cleared on panel close.
Apply persists literal colors and version-1 saturation controls together in schema55.
These controls are not yet a complete harmony-wheel recipe.

Current literal HEX is converted to OKLCH; lightness/hue/alpha are retained within
8-bit conversion precision. Achromatic colors keep their original literal rather
than acquiring an arbitrary hue. Dark and near-white slots receive Text and
Background weights respectively, with Primary for other slots. Those are temporary
heuristics, not semantic roles authored by the user. Area compensation uses a deep-copied current coverage snapshot when available.
The checkbox can disable it. A missing, pending, failed or stale measurement is
explicitly labeled unweighted; it is never substituted with older coverage.
A preview that used coverage cannot be applied after its scene/font key changes.
Contrast and semantic guards remain unimplemented and the UI states that limit.

## Saved saturation controls (schema55)

Optional project-level `colorDesign` has exactly version=1, profile, colorfulness,
areaAware and lockedSlotIds. Locks reference existing stable color-slot IDs, never
names or array positions. The optional `design` payload on `set-color-slots` is
validated with every color update before mutation and saved in the same transaction.
One Undo/Redo restores both colors and controls. Commands without `design` retain
the existing no-op/Redo semantics.

Saved design changes, Undo/Redo and a new document reload the controls. An unrelated
literal change preserves the local draft; Preview/Cancel still do not author it.
Whole native project save/reopen retains controls, while importing selected nodes
or compositions keeps the receiving project's design and imports referenced color
values rather than replacing its palette policy. Full wheel relationships, role
authoring and contrast guards remain separate unfinished work.

Schema56 version2 extends the design with discrete wheel settings and stable-ID
palette coordinates/roles. The same preview keeps current alpha/Keep literals;
managed chromatic slots use their authored wheel coordinates rather than feeding
quantized output colors back into the next proposal. Unmanaged shared colors retain
the existing current-color behavior. UI Apply preserves version2 rather than
silently downgrading the recipe to version1.
