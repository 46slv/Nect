# Shared manual colors panel

This first UI slice exposes the schema 54 shared-color registry. It is not the
automatic palette/AutoChroma design.

- The right workspace bar opens Shared Colors above Inspector. Opening and
  closing it changes view state only; the existing Assets portal remains mounted.
- Add, name, HEX and alpha edits use typed Session commands. Each accepted edit
  is one Undo step. Empty or invalid values do not become authored data.
- Each eligible Inspector Color field has its own stable-ID selector. Equal names
  or equal visible colors do not merge references. Connected, expression-driven
  and locked parameters are unavailable for binding.
- A bound field displays its resolved value and disables local literal edits.
  Unlinking materializes the current shared value, not the old fallback value.
- Controls carry their rendering Session identity and revision. Detached controls,
  old document controls and old revisions cannot mutate the current document.
- Shared slots and bindings are document/project data and survive JSON reopen.
  Panel visibility is local view state, not an authored document setting.

Validation is split deliberately: controller DOM tests and core command tests
cover routing/identity/history; the browser acceptance checks actual panel and
Inspector interaction, live output, browser download/reopen, retained Assets and
narrow layouts. Browser file inputs are not Windows native dialog acceptance.

Visible-area percentages are derived asynchronously while the panel is open.
Sampling uses the actual renderer's solid fill/stroke and strict two-stop linear
or concentric radial fill passes and source-over contributions, never RGB matching.
Gradient stop IDs come from authored bindings (including Color-node sources);
unbound stops remain Other even when their HEX matches a shared slot. For the
unpremultiplied sRGB ramp, the two stop contributions are the actual paint alpha
A(t) times (1−t) and t, not each stop's own alpha times its weight. A transparent
stop can therefore still supply hue to mixed pixels. Both contributions occlude
the backdrop once, as one paint. Radial t is min(distance / radius, 1) in absolute
artboard pixels, using the production radial renderer for the private weight mask.
Focal/elliptical/extra-stop/angular paints, gradient strokes, authored masks,
filters, blend modes, independently grouped paths and repeater appearance remain
unsupported; the whole scene becomes unavailable rather than partly measured.

The longest sample edge is at most 256 pixels;
aggregate alpha storage is capped at 4,194,304 samples. Percentages are approximate,
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
fixtures cover linear and radial ramps, inside/outside radial centers, circular
distance and pad, transparent/unequal alpha, clipped and antialiased geometry,
equal/missing IDs and overlap/stroke order. Their independent oracle uses
analytical ramps and forward source-over, with 16×16 polygon-edge integration
for the radial AA fixtures; it does not use the production mask or coverage reducer.
Configured assertions are not a passed browser run: results must be checked for
the exact published commit. Native WebView2 acceptance remains separate.
