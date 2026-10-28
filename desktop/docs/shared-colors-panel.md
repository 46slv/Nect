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
Sampling uses the actual renderer's solid fill/stroke passes and source-over
contributions, never RGB matching. The longest sample edge is at most 256 pixels;
aggregate alpha storage is capped at 4,194,304 samples. Percentages are approximate,
exclude transparent pixels, and retain unbound paint as Other. Unsupported scenes
or exceeded budgets display unavailable rather than partial or stale percentages.
Session/revision/font identity guards reject old results. Output text patches do
not replace focused inputs or interrupt composition. Decode resources are released
on success, failure and timeout. There is no background sampling while closed.

Remaining work includes automatic palette generation and constraints, broader
renderer attribution, and Windows app acceptance. Do not describe the current
manual registry and bounded solid-paint measurement as completing those requirements.
