# P02-D Packet C — compact Inspector feedback qualification

## Authority and boundary

- Packet: [Nect UI Closure Packet — Binary Icon Toggles / Angle Coverage](https://app.notion.com/p/3eafd279a6f38185bce6e73fcb46b0df)
- Requirement: [REQ-45 — pointer UI is fluid / proximity hover](https://app.notion.com/p/3e0fd279a6f381f3adf3e90cbd811a31)
- Source baseline: `46e84fd72988561b2ea04b9da8fcadf31332e87f` (`dot/d0-cloud-canary`)
- Packet C was limited to the existing compact Inspector controls and their labels/tooltips. It adds no document state, data schema, gestures, geometry changes, animation framework, or utility-icon implementation.

## Census and fail-only repairs

The source census covered property value rows, the expression and source-pick actions, operation stack movement/removal, and the available single and batch primitive/point/Repeater angle controls.

- The source-pick action originally had an action-only tooltip overwritten by the drag instructions and no accessible name. It now names the target property in both tooltip and accessible name while retaining the existing drag, hover-to-inspect, and click-to-search instructions.
- The `fx` control already had a target-oriented accessible name, but its tooltip described only the expression interaction. Its tooltip now includes the same property name. It has no `nect-reference`, `nect-targets`, or pick-whip metadata; it remains ineligible as a dropped property source.
- Stack-up and stack-down buttons now expose the operation label and action in tooltips and accessible names. The remove action retains its operation name and clarifies that it removes from the stack. Boundary enabled states are unchanged.
- Single Repeater, primitive, and point angle controls, plus primitive, point, and Repeater batch dials, now include the target property/subject in hover feedback while retaining their interaction, direction, exact-value, cancellation, and driven-state guidance. Existing labels and action semantics are reused.
- Utility Guide/Grid/Snap controls were not changed or reimplemented.

## Verification

- New `compact_inspector_feedback_ui_tests` was registered at normal and 2× device scale. It checks the compact-control census, tooltips, accessible names through `QAccessible`, operation boundary enable states, single and batch angle subjects, and exact Ref-vector use.
- The source-pick regression drops a pick-whip onto the expression button only after ensuring the two controls are in the Inspector viewport and verifying deep hit testing resolves to `fx`. The expected `NO_SOURCE` result and unchanged native bytes, revision, and History prove the expression button was not broadened into a property source.
- The test drives synthetic `QEnterEvent` and `QHelpEvent` feedback requests, keyboard focus, enabled-state transitions, and an Inspector rebuild; it compares child/parent global geometry and checks document bytes, revision, and full History. Both normal-scale and 2× offscreen CTest cases passed. This is synthetic Qt geometry/accessibility evidence, not OS-level pointer/proximity or visual-contrast acceptance.
- Focused angle/utility plus compact-feedback CTest: 19/19 passed across interaction, high-DPI, and existing fractional-DPI cases.
- Release all-target build: passed.
- `python3 scripts/smoke.py --exe build/d0-core/nect`: passed; fresh-process native validation and SVG anchor checks passed. The smoke result explicitly reports `gui_tested: false` and `mcp_tested: false`.
- Full CTest after the measured caption-copy repair: 91 total, 64 passed, 1 skipped, 26 failed. The 26 failed names exactly match `build/d0-evidence/repeater-batch-final-full-tests.log` (89-test baseline); no new failure was introduced. These counts distinguish passing, skipped, and failed tests rather than treating the printed percentage as a passing-test count.

## Evidence and acceptance limits

- Focused/full logs: `build/d0-evidence/p02-d-packet-c-caption-final-full-tests.log`, `build/d0-evidence/p02-d-packet-c-caption-final-smoke.log`
- Full-suite failed names: `build/d0-evidence/repeater-batch-final-full-tests.log`
- A separate Windows metrics-only diagnostic used Yu Gothic UI 9.75pt at a 98×80px caption allocation and 17px line spacing. The chosen visible three-line pattern is `Common · count`, direction (`+X zero` or `top zero`), then bounded `Δ value°`. The measured candidates fit at normal, 2×, and 1.25× DPI: `Common · 3 / +X zero / Δ +999.9999°` line widths 71/47/78px normal and 70/45/78px scaled; `Common · 12` widths 78/47/78px normal and 77/45/78px scaled; `Mixed · 32 / top zero / Δ -999.9999°` widths 61/50/74px normal and 59/48/74px scaled. Reported `heightForWidth(98)` was 80px for each measured compact sample. Diagnostic logs were `caption-layout-final-normal.log`, `caption-layout-final-dpr2.log`, and `caption-layout-final-fractional.log`; they also report the fixed-global geometry checks passing. Counts 3, 12, and 32 are only tested examples, not an application selection limit or maximum proof.
- The visible caption omits the noun and the word “relative” to fit. Its accessible name carries the full Common/Mixed subject, target noun/count, direction, exact unwrapped value, and 17-digit relative delta. The dial accessible name carries the property/target subject and noun/count; its accessible description carries the live exact current value and delta. Tooltips name the action/property and retain stable orientation, interaction, driven-state, and cancellation guidance; tooltips are static and do not claim a live delta. The row stays 80px and the dial hit target stays 44×44px. Extreme larger-count strings and scientific-notation delta limits were not measured, so no universal fit claim is made for them.
- The Windows-gated Repeater test compile issue was corrected by replacing the undeclared `mixed_domain` reference with the already declared ordered `expected` Ref vector. The patched-candidate Windows build/runtime requalification is still pending. No real GUI/O1 run, human motion/visual acceptance, screen-reader action/value test, true OS hover/proximity input, driven-property tooltip fixture, or outgoing-handle-specific fixture is claimed by this receipt; Packet C's exact-candidate O1 acceptance remains pending those separate gates.
