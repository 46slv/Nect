# Shared manual colors panel

This UI exposes the shared-color registry and the existing AutoChroma controls.
The opt-in Key color trial has a separate, versioned recipe contract.

- The left workspace bar opens Shared Colors. Opening and
  closing it changes view state only; the existing Assets portal remains mounted.
- Add, name, HEX and alpha edits use typed Session commands. Each accepted edit
  is one Undo step. Empty or invalid values do not become authored data.
- Each eligible Inspector Color field has its own stable-ID selector. Equal names
  or equal visible colors do not merge references; duplicate names receive a
  distinguishing ID-derived suffix in the selector. The same applies when a
  shared name would collide with “Local color”. Canonically equivalent Unicode
  names are disambiguated for display without changing the saved spelling.
  Connected, expression-driven and locked parameters are unavailable for binding.
- A bound field displays its resolved value and disables local literal edits.
  Unlinking materializes the current shared value, not the old fallback value.
- A row's Actions menu can delete an unused shared color as one Undo step. A color
  used by any Composition, saved node preset, legacy palette/lock or retained or
  live Key color recipe is rejected before mutation. Project metadata that merely
  contains a `colorSlotId` string is not a binding. A committed removal expires
  any copied reference even if Undo later restores that ID.
- A color row can copy a value or a reference inside Nect. A value is the exact
  HEX/alpha at copy time and can be pasted as an independent literal; a reference
  keeps its stable slot ID and can be pasted only while that slot exists in the
  same open project, including another Composition. Replacing the project
  invalidates a copied reference even if the new project has the same ID. A
  committed removal also expires the reference; restoring or reusing that ID
  requires copying it again. The
  temporary in-app copy does not read or write the system clipboard. Pasting over
  a bound Color parameter explicitly replaces its link with the copied value in
  one Undo step. The same Color-parameter controls are available for Stroke,
  gradient stops and a Fill applied to Text; Text character-specific paint is not
  a supported paste surface.
- A row's Actions menu can find direct uses across the current project. The list
  contains typed node Color parameters bound to that exact slot ID, not equal
  literals, expression dependencies or inferred visible coverage. Choosing a use
  opens its Composition, Layer and node and focuses the Color field when active;
  this is view navigation and adds no History. A changed project, removed target
  or active draft rejects navigation instead of retargeting a stale result. Long
  names are shortened in the list; their full saved text remains available on
  demand and the underlying identity never uses the shortened label.
- Controls carry their rendering Session identity and revision. Detached controls,
  old document controls and old revisions cannot mutate the current document.
- Shared slots and bindings are document/project data and survive JSON reopen.
  Panel visibility is local view state, not an authored document setting.
- Sort by name changes only this panel's view while the project remains open.
  The toggle restores registry order; opening a different project resets it.
  It does not alter saved slots, palette order, History or coverage calculations.
- Find filters the saved-color rows by name in the current project view. Matching
  ignores case and Unicode composition differences; closing Find restores every
  row. The query resets when another project opens and never changes saved data.

Validation is split deliberately: controller DOM tests and core command tests
cover routing/identity/history; the browser acceptance checks actual panel and
Inspector interaction, live output, browser download/reopen, retained Assets and
narrow layouts. Browser file inputs are not Windows native dialog acceptance.

Visible-area percentages are derived asynchronously while the panel is open.
Sampling uses the actual renderer's solid fill/stroke and strict two-stop linear
or concentric radial fills, plus the existing four-stop angular fill, and
source-over contributions, never RGB matching.
Gradient stop IDs come from authored bindings (including Color-node sources);
unbound stops remain Other even when their HEX matches a shared slot. For the
unpremultiplied sRGB ramp, the two stop contributions are the actual paint alpha
A(t) times (1−t) and t, not each stop's own alpha times its weight. A transparent
stop can therefore still supply hue to mixed pixels. Both contributions occlude
the backdrop once, as one paint. Radial t is min(distance / radius, 1) in absolute
artboard pixels, using the production radial renderer for the private weight mask.
Angular uses the production renderer's 720 midpoint wedges and premultiplied
interpolation: each stop contributes its own alpha times its interpolation weight,
so a fully transparent angular stop contributes zero (unlike SVG linear/radial).
Four private white/black stop-basis renders preserve every wedge's original alpha
as well as the exact pattern/geometry bounds, angle, direction, stop positions and
crispEdges, without changing product rendering. The decoder reads premultiplied
red (canvas R × A) to recover each visible contribution, including source-over
between quantized overlapping wedges. Changing wedge alpha instead is incorrect:
two opaque 50/50 wedges overlap to full alpha 1, while independently rendering each
stop at alpha 0.5 would sum to 0.75 + 0.75 = 1.5. No outer alpha mask is multiplied
again; geometry antialiasing and wedge occlusion have already been applied.
Mixed-ID angular basis decoding requires an actual sRGB float16 canvas plus actual
rgba-float16/Float16Array readback, checked rather than assumed from requested
options. There is no unorm8 fallback or conversion claimed as equivalent. Ordinary
solid/linear/radial and identical-ID angular decoding remain unchanged. The full
paint's original alpha remains the denominator; only relative contribution data
uses higher precision. CI records actual context/readback precision and bounded
missing-pixel mass diagnostics. This is one bounded experiment: if actual Chrome
cannot prove float16 support and recover the distinct-ID AA fixture's component
mass, mixed-ID angular support is withdrawn rather than repeatedly changing
thresholds, inventing residual contributions, or assigning them to Other.
Their masses share one full-paint alpha and occlude the backdrop only once.
When every validated stop ID is identical (including all unbound), linear, radial
and angular paints are attributed directly to that identity without auxiliary
masks. This removes redundant SVG generation, decode and sampling-budget charges;
it is exact even at faint antialiased edges and never compares HEX values. All
stop identities and the production paint are still validated before use.
Production wedge colors remain 8-bit; their color rounding and float16
composite/readback differences are reconciled proportionally only within 4/255 per
pixel. If a painted pixel has no recoverable component mass (including near-zero
alpha/AA-edge precision cases), or the discrepancy exceeds that bound, the entire
measurement is unavailable. No mass is invented, assigned to Other, or partly shown.
Independent contour groups with solid, two-stop linear/radial or four-stop angular
paint are measured in the renderer's per-group fill-then-stroke order, including
fully opaque 2D Repeater copies. A group's contours stay together so holes keep
their original fill rule. Each isolated angular pass reuses a pattern built from
the whole evaluated path, matching the production pattern bounds and radius.
Partial Repeater opacity is measured only when every paint in the scene is a
solid fill and there are no strokes. Each group keeps its original SVG opacity,
including zero and fractional copies, and occludes the backdrop once. Partial
opacity with a stroke or gradient, alternate paint, focal/elliptical/extra-stop
paints, gradient strokes, authored masks, filters and blend modes remain
unsupported; the whole scene becomes unavailable rather than partly measured.

The longest sample edge is at most 256 pixels;
aggregate alpha storage is capped at 4,194,304 samples, counting the full angular
paint and all four contribution masks. Percentages are approximate,
exclude transparent pixels, and retain unbound paint as Other. Unsupported scenes
or exceeded budgets display unavailable rather than partial or stale percentages.
Session/revision/font identity guards reject old results. Output text patches do
not replace focused inputs or interrupt composition. Decode resources are released
on success, failure and timeout. There is no background sampling while closed.

Remaining work includes automatic palette generation and constraints, broader
renderer attribution, and Windows app acceptance. Do not describe the current
manual registry and bounded paint measurement as completing those requirements.

Coverage verification is split between `color-coverage-passes.test.js` (strict
descriptors, exact provenance, history/reopen, order and nonmutation) and
`color-coverage.e2e.js` (production SVG decoding and canvas alpha). The browser
fixtures cover linear/radial ramps and four-stop angular wedges, inside/outside
centers, angular rotation/direction/nonuniform positions, circular
distance and pad, transparent/unequal alpha, clipped and antialiased geometry,
equal/missing IDs and overlap/stroke order. Grouped angular Join and Repeater
fixtures also compare each stop's per-pixel mass and alpha with independently
recolored production SVG, including holes, AA and single-group parity. Their
independent oracle uses analytical ramps and forward source-over, with 16×16 polygon-edge integration
for the radial/angular AA fixtures, and explicit midpoint-wedge premultiplied
alpha weights for angular paints; it does not use the production mask or coverage reducer.
For the fixed opaque-center angular fixture, an independently constructed ordered
720-wedge reference SVG models primitive boundary rasterization and source-over;
the ideal-angle oracle remains strict on the interior excluding only its 28
positive-ray/center pixels. Whole-fixture totals and explicit seam/center probes
use the independent primitive reference, without widening tolerances.
Configured assertions are not a passed browser run: results must be checked for
the exact published commit. Native WebView2 acceptance remains separate.
