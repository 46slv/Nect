# Harmony wheel model (unconnected foundation)

`harmony-wheel.js` implements the accepted v1.1 mock's Triadic, Complementary,
Analogous and Split Complementary cell relationships. It is a pure model, not a
panel or saved recipe. No production UI imports it yet.

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
can differ from a grid cell's swatch. Full control wiring, drag interactions,
recipe persistence and contrast/semantic guards remain unfinished.
