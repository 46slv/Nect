# Color operations v1

Base: PR43 `4cfe52eb2d5e66c34bbb2bfab24a422a38ccd967`, branch
`codex/selected-point-bounds`. Sole core/main writer. PR43 EXE/receipt sealed at
`C:/Users/shiro/Documents/Codex/2026-10-21/task-13/selected-point-bounds-artifact-f9a1b95`,
SHA256 `a5575c07e893fbc138cb6fcb0df838f86e713d452120c8c859e1040c696ce72b`,
verified before a new build. Preserve PR40–43 sealed candidates.

Source: [Color operations v1 implementation decision](https://app.notion.com/p/400fd279a6f3813591ced336273c60c6),
read at its 2026-10-21 13:02:33 revision. Current Color grammar is value/ref only;
mix/alpha are absent. Add just `mix(Color, Color, Scalar)` and
`alpha(Color, Scalar)`, retaining Base/Expression/Result and stable same-composition
references. No palette, OKLCH, input/compose, arbitrary RGBA construction, new
panels or unrelated audits.

Decisions: one pure shared Color operations module; linear-light sRGB and
premultiplied interpolation; alpha replaces rather than multiplies. Finite t/a
must be within 0–1 or reject. Six-digit `#RRGGBB` Color literals support the
specified numerical examples. Existing Scalar arithmetic can supply weights;
Color never coerces into Scalar/Vector2. Carry linear float Color through nested
operations and quantize only at the public parameter boundary. References read
their target's public result, so a reference is an intentional boundary. Preserve
hidden RGB in saved transparent base/alpha replacement; only zero-alpha mix
results normalize to transparent black.

Schema43 pins v1's new Color grammar/semantics; upgrade only when saving new
operators/literals. Schema42 value/ref-only documents and their base values are
unchanged. Older readers must reject schema43 clearly. Keep connection priority,
dormant expressions, typed dependencies/cycles and existing atomic Session/Undo
contracts. Scalar weight references need their own dependency type rather than
the current assumption that every reference inside a Color expression is Color.

Focused acceptance: all specified numerical oracles, especially nested
`mix(mix(#000000,#333333,0.5),#000000,0.5)` = #161616 (reject intermediate-rounding
#171717); transparent endpoints/hidden RGB, type/range/missing/cycle/stale errors,
dependency cache/update, one-Undo base/expression edits, JSON roundtrip and legacy
Color/Scalar/Vector2 regressions. Exercise existing Linear/Radial Color controls
in the native WebView2 candidate. Report unavailable physical/OS-dialog/cold
reopen coverage separately.
