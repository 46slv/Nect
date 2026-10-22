# Luminance native WebView acceptance

2026-10-22. PR73 follow-up on receipt HEAD
`c5fce8045aa5fd70c76939629c8545c60e5faa5e`; product remains
`2e3de3a1fec6bffe0d41ca37a670f15a535d2579`.
Only an acceptance script and documentation changed. No bug found or EXE rebuilt.

## Runtime and checks

Used the sealed10,025,472-byte EXE, SHA-256
`04678c6585fad8994a463529540b71051722e81693d9357731a2e82597cea7ac`,
from `mask-luminance-artifact-2e3de3a`. A new runtime copy matched that hash.

The native application ran in a dedicated disposable profile with synthetic
fixture data. No production document or Recovery dialog was used.

`scripts/verify-mask-luminance-native.mjs` drove actual native WebView controls
via CDP synthetic input. The six states were Alpha, Luminance, Keyer Luminance,
Keyer Luminance+Invert, Keyer Luminance+Feather4, and that Feather result+Invert.
All used a new180x80 synthetic document with embedded PNGs.

- Every state's actual Canvas layer markup and the same exported SVG rendered
  to exactly equal full RGBA. CLI wrote the WebView-produced PNG bytes.
- The Luminance state also matched the sealed headless equal-luminance oracle:
  RGBA SHA-256 `3942951ca8533f5572135627a27c15d8ad241f3c1b578699452eaac85d8ccee4`;
  PNG SHA-256 `edaad704716549a5d11c60d2dd71daeaa1e8922e60bf3d86e75e949015f253b5`.
- RGB[94,148,30] and RGB[162,108,226] gave equal output RGBA and alpha128/128
  at full material alpha,64/64 at half alpha. Partial target/material alpha
  produced64 in Alpha and32 in Luminance. Fully transparent white gave0, and255
  after Invert at sigma0. Keyer removed the green band before Luminance.
- Feather changed edge coverage; Feather/Invert alpha sums matched source
  alpha with maximum error0 over every pixel. Native default Canvas reported
  srgb/unorm8 and Uint8ClampedArray.
- Alpha/Luminance switches, material selection, Feather and Invert edits each
  advanced one revision and restored exact document+SVG with Undo/Redo.
  Alpha back-switch restored the original SVG. Repeated values were neutral.
- CLI saved a10,150-byte project backup in the dedicated profile documents
  directory, read it back exactly and loaded it with the existing debug loader.
  Project, stable IDs, settings and SVG matched exactly. Backup SHA-256:
  `de020c3989e018b63ce3f893405ad9ee687966cb33c55767854864900f9f9494`.
- Native screenshot visibly showed Luminance(sRGB), Feather4 and Invert with
  the expected softened mask. No page errors were recorded.

After readback, the existing `finish_close` IPC closed the page. A subsequent
process/listener inspection found the dedicated runtime and its descendants
absent. This checks bounded shutdown, not OS file-dialog behavior.

The acceptance script records six JSON/SVG/Canvas-SVG/PNG cases, numeric oracles,
project readback and close. The observed results above are historical checks;
this documentation update does not execute the native script again.

## Remaining boundaries

Product: Image-typed material (Image/Keyer) supports Alpha or fixed8-bit encoded
sRGB Luminance. Closed Path retains geometric coverage; arbitrary Composition/
PaintedPath material, linear/ICC/AE color equivalence and a general mask stack
are outside this slice. Existing Image Position/Size and post-Mask appearance
Transform remain the supported placement routes; no arbitrary pre-Mask Image
Transform route was added. Feather sigma remains0..256 document units with
existing filter-region limits (side8192, area16,000,000) and atomic rejection.

Verification: CDP controls are synthetic, not physical/OS input. The backup was
written/read with CLI and reopened by the same native process's debug loader;
fileState was dirty and unbound before close. This does not accept normal OS
Save/Open, clean-save/close or cold reopen. Native Export PNG's save handler and
OS PNG dialogs were not invoked. PNG evidence is the native WebView's common
SVG rasterization with CLI byte writes. Previously completed geometry/transform
checks and focused/headless suites were not repeated. No installer or release acceptance is implied.
