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
