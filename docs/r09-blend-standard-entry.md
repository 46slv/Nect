# R09-BLEND-STANDARD-01 — transparent standard blend qualification

Status: Sol-frozen bounded qualification packet, 2026-10-01. Entry is clean synchronized `codex/practical-alpha@a90bfcb6154cda1c09a6561f1fda30c5c31e9f02`, native writer 0.72. Authority: DEC-71 Mission, Completion Route R09/P09 and the accepted G8 standard-blend baseline. This packet qualifies only the 12 currently supported separable W3C-overlap modes in the declared 8-bit sRGB Canvas; it does not add a mode or assert After Effects compatibility.

## Existing evidence and gap

`tests/compositing_canvas_tests.cpp::all_supported_blends_match_independent_channel_formulas` already checks all 12 modes against separately written W3C channel formulas on an opaque backdrop with source opacity 1 and 0.5. It does not exercise partially transparent backdrop and source together, where Porter-Duff alpha and blend terms both contribute. `src/desktop/canvas.cpp::blend_mode` maps those modes to Qt composition modes.

## Oracle and acceptance

Use one bounded transparent-background RGBA fixture with distinct mixed-channel source and backdrop colors, each with nontrivial 8-bit alpha. For every supported mode, derive expected output independently of Qt from [W3C Compositing and Blending Level 1, §6 and §10.1](https://www.w3.org/TR/compositing-1/):

`ao = as + ab * (1 - as)`

`co = as * (1 - ab) * Cs + as * ab * B(Cb,Cs) + (1 - as) * ab * Cb`

`Co = co / ao` when `ao > 0`, using straight sRGB channel values for `Cs`, `Cb`, and the existing independent `B` functions. Record source/backdrop bytes and bounded Qt 8-bit rounding tolerance; compare both output alpha and RGB for all 12 modes. A transparent-root sample and an outside-overlap sample must show no implicit white UI backdrop. Verify Group isolation order and source opacity are applied once. If a real renderer mismatch appears, repair the bounded renderer behavior and rerun relevant tests; do not widen tolerances merely to pass.

Focused Canvas test plus the existing compositing regression are the qualification gate. Preserve A1/A2 mask oracles and native/authored state. No native version, API, UI or new dependency is needed if existing modes conform. Actual AE mode comparisons, opaque production color management, nonseparable hue/saturation/color/luminosity, high-bit-depth and hands-on GUI acceptance remain separate work.
