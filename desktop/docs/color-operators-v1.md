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

## Receipt

Product source `c9ea67669d9e9a7a0f8a63dfc517d561685d39c4` implements the shared
linear-float module, typed Scalar/Color dependency traversal, Desktop schema43,
pure Color draft validation, context-aware completion/reference types and a
captured revision for Color dialog Apply. No packages, palette, new panel,
input/compose, point editing or Image/Shape dither behavior changed. Existing
schema monotonicity is retained: removing a new operation does not downgrade a
document; Undo to the earlier snapshot restores its previous version.

- **PASS:** 85/85 focused checks across new Color operator math/DOM/completion,
  existing Color value/ref, authored base, Scalar/Vector2 math and stable
  parameter/reference suites. Includes all numerical oracles, #161616 versus
  #171717, transparent mix black normalization versus hidden base RGB,
  alpha replacement, finite/range/type/cycle/missing/stale rejection, neutral
  preview/Cancel/no-op, one-Undo base/formula edits, dependency cache reuse,
  rename/reorder/duplicate/preset references and JSON roundtrip.
- **PASS:** the exact PR43 reader preserves a schema42 value document
  byte-for-byte and rejects schema43 with `Unsupported project version`.
  A first probe needed bare package imports resolved to file URLs; this was a
  Node probe setup correction, not an import-format product repair.
- **PASS:** actual native Tauri/WebView2 Linear and Radial Gradient flows:
  value/ref/mix/alpha entered in existing Color fields; typed completion;
  alpha replacement with transparent red Base retained; Color-source alpha
  and Scalar weight edits update dependent results; nested #161616;
  one-Undo/Redo, no-op and invalid/Cancel neutrality; disk JSON read/write plus
  renderer reload. Canvas stop colors, SVG and Result use identical public
  hex values (#161616 and #8900e1). Zero page errors; both screenshots inspected.
- **PASS:** production frontend (140 modules), offline Windows native release
  with custom protocol, native config, scoped formatting and diff checks.
  The existing bundle-size warning remains.
- Native EXE: 9,997,824 bytes; SHA256
  `d8d325786f0d20b6630261125271d495b480f021cdf036c5a2b4b17682fbcd22`.
  Embedded `index-C4pt1KUx.js` SHA256
  `cc182e55c77b9b9f662d9cdda0c1b852160e8926719fc1e298e4eaacecf6fd53`.
  Owned PID141756, isolated `nect-svg-acceptance-color-ops-r1` profile.
- **NOT RUN:** unrelated full audits, separate browser rerun, native OS
  Save/Open dialogs, cold-process file reopen and physical keyboard/IME.
  Native inputs were synthetic Playwright CDP. Disabled native CUA and
  unavailable Windows node_repl routes were not bypassed.

Ignored evidence under `desktop/test-results/color-operators/`: final 85-check
log, legacy-reader probe, frontend/native build logs, native interaction log,
`native/receipt.json`, Linear/Radial screenshots and JSON fixtures. Earlier
local logs include two corrected test expectations: alpha is now supported,
and a Scalar reference named value is a valid completion in a weight slot.
These are distinct from the final passing run.
