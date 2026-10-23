# Luminance mode verification

Base PR72 HEAD: `8ab305a33ecf9f8dade2742ec082dbaa2fce0373`.
Product commit: `2e3de3a1fec6bffe0d41ca37a670f15a535d2579`.

- 51 focused tests passed, zero failures/skips: Luminance model/DOM and existing
  Image-alpha/Feather/Image effects/Composition Reference coverage.
- 9 production-Web/CSP headless documents passed JSON reopen and exact full RGBA
  equality across actual Canvas layer markup, SVG and web Export PNG downloads.
  The default Canvas reported sRGB/unorm8 and Uint8ClampedArray.
- Expected material coverage in8-bit units was exactly
  `[0,255,128,54,182,18,128,64,0]` for black/white/gray/R/G/B/half-white/half-gray/
  transparent white. With partial target alpha, Alpha versus Luminance gave64/32.
- Different RGB `[94,148,30]` and `[162,108,226]` have the same exact weighted
  numerator1,280,000. Both produced128 at full alpha and64 at half alpha, with
  matching output RGBA. Bright fully transparent white at Feather0 produced0
  before Invert and255 afterward. Normal/inverted complement error was zero.
- Keyer-processed material, document placement, later appearance transform,
  Feather order, Undo/Redo and neutral repeat/default edits passed. Existing
  Source RGB interiors/assets remain unchanged. Invalid modes, locks, stale
  commands and cycles reject atomically. No assertion was removed or skipped.
- 21 prior JSON/SVG documents remained byte-identical. All183 sealed files from
  PR71/72 and the completed native follow-up matched before/after building.
  No prior native runtime test was repeated.
- D frontend/config checks and offline custom-protocol native compile passed.
  The first fully D source/cache/target build recompiled dependencies with2 jobs
  and completed in10m45s. EXE10,025,472 bytes; SHA-256
  `04678c6585fad8994a463529540b71051722e81693d9357731a2e82597cea7ac`.
  At product freeze it was compiled, not launched. No installer was built.
  The bounded native follow-up below now verifies this same EXE.

C initially had0 bytes free. The old checkout was not moved or edited; this task
uses a new remote-backed checkout at
`D:\Nect-codex-output-20261022-task13-f336c94f2b714cd5bc73d6ca646c8f2e\mask-luminance-20261022\Nect`.
Installed Node dependencies and cached Cargo registry were copied for D-local
builds. No software install, credentials/global config copy, guard change,
existing artifact deletion or global setting change occurred. These caches are
outside Git and remain available for subsequent development.

Working evidence is in the enclosing `mask-luminance-20261022` directory. The
separate `mask-luminance-artifact-2e3de3a` directory seals the EXE, bundle,
editable Alpha/Luminance JSON/SVG examples, PNGs, logs and SHA-256 manifest.
See [the contract and baseline references](mask-luminance.md) and
[the comparison guide](../examples/mask-luminance/README-ja.md).

This is the G8 R09 initial8-bit sRGB profile for Image/Keyer mask material. Path
geometry stays on its existing route. DEC-53 Proposed/REQ-241 Candidate do not
extend scope. Linear/ICC/AE equivalence is not claimed. Luminance-specific native
WebView acceptance is now complete in the bounded follow-up below. Physical/OS
input, normal OS Open/Save/PNG and cold reopen remain unverified. JSON/project
roundtrip is not OS save/reopen acceptance.
No normal production document/Recovery, main merge, release or Library upload.

## Native follow-up, 2026-10-22

The same sealed EXE passed six Luminance-specific native WebView states via CDP
synthetic controls. No product change or rebuild was needed. This did not rerun
the51 focused tests,9 headless documents or prior native acceptance. See
[the native receipt and D-checkout handoff](mask-luminance-native-verification.md).
