# Harmony wheel model and preview panel

`harmony-wheel.js` implements the accepted v1.1 mock's Triadic, Complementary,
Analogous and Split Complementary cell relationships. The pure model now backs Shared Colors
version-2 recipe controls (schema56). Browser/native acceptance must be checked
separately from the geometry and DOM tests.

The five closed settings are harmony, hueSteps (3–72), hueRotation (bounded
degrees), lightnessSteps (1–12), and lightnessRange (0–1, centered on 0.5).
Incompatible cell counts are rejected, never rounded to a nearby harmony.
Complementary keeps the mock's repeated opposite cell for positions 1 and 2;
those positions may use different lightness rings.

Ring indices are zero-based, inner/bright to outer/dark. Points lie at exact ring
centers and hue-cell centers. Angle and radius are geometry, while hueRotation
changes sampled hue only. Geometry uses unit radius, center (0,0), and degrees
with position 0 pointing upward. Cells expose start/end angles and inner/outer
radii; each cell has one solid HEX value, not a gradient.

Palette inputs identify each output with a stable ID and an explicit harmony
position, ring and role. Reordering inputs does not retarget a color's position.
Additional slots can explicitly reuse a harmony position or be neutral slots with
an explicit lightness. Protected literals and alpha are preserved by the existing
AutoChroma engine. Neutral slots have zero chroma unless a protected literal takes
precedence. Coverage must describe the supplied palette identities; callers must
explicitly aggregate unrelated identities into Other rather than silently guessing.

The derived visual grid uses bounded proposal batches and can contain more than
256 cells; the authored palette remains limited to 256 colors. Visual grid colors
use Primary weighting without area correction. Role/area-weighted palette outputs
can differ from a grid cell's swatch. The panel supports radial ring drags, keyboard/selected-color alternatives,
controlled drafts, and atomic recipe/color Apply. Contrast/semantic guards remain
unimplemented. Native dialog and screen acceptance are separate.

## Authoring boundary

Create palette adds three chromatic colors and two neutrals without replacing
existing colors or bindings. Add palette color appends a stable ID. Both actions
are one Session transaction; explicit invalid kinds and budget overflow reject.
Draft changes must be applied or reloaded before adding colors, preventing silent
draft loss.

Schema56 `colorDesign.version=2` adds a closed wheel settings object and palette
entries to version1's profile/colorfulness/area/Keep fields. Palette entries refer
to existing IDs, not names or registry positions. Geometry choices and roles
persist together with resolved literal colors. Imported compositions retain the
receiving policy. Pointer/keyboard edits and preview are view drafts until Apply.
Shared Colors scroll position is preserved across renders, so a partially scrolled
wheel does not jump during dragging. Old fields cannot retarget a newly selected
color. Full renderer attribution, print/ICC and universal design correctness are
not claimed.

## Optional per-color Chroma

Schema57 extends version-2 chromatic palette entries with optional `overrideC`,
using the existing OKLCH engine's absolute 0–0.45 range. An absent field means
Auto (global): profile, colorfulness and area weighting remain the default.
An explicit zero is an override, not inheritance. Override Chroma bypasses those
soft weights but still respects the existing sRGB gamut boundary; the requested
number persists and a gamut-limited preview is labeled. Hue/ring geometry is
unchanged. Keep literals take precedence, including their exact alpha.
Neutral entries cannot carry this field and continue to resolve with C=0 unless
an explicit Keep literal takes precedence.

The selected chromatic slot offers Auto (global) or Override. Enabling Override
starts from the current literal's Chroma; the numeric field is shown only then.
Changing it is a local draft. Preview and Cancel never alter authored state;
Cancel preview retains the local draft, while Reload saved settings discards it.
Apply commits colors and the stable-ID recipe in one existing Session history
step. Numbered marker interiors show saved/proposed slot RGB, independently of
the globally weighted wheel cells. No automatic Guard, new harmony family,
continuous-angle editing, dependency editor or aesthetic guarantee is added.

Schema54 shared colors, schema55 version-1 recipes and schema56 version-2
recipes retain their existing semantics and do not upgrade merely by opening,
previewing or editing a view draft. Only an applied explicit override requires
schema57. Schema56 data containing an override field is rejected rather than
silently ignoring it; returning to Auto deletes the optional field without
lowering the document schema. Save/reopen, both Document/Composition sessions,
Undo/Redo and stale/invalid controls are covered by focused tests. Browser and
native screen acceptance remain separate from these model/DOM tests.
