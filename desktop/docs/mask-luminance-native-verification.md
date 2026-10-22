# Luminance native WebView acceptance

2026-10-22. PR73 follow-up on receipt HEAD
`c5fce8045aa5fd70c76939629c8545c60e5faa5e`; product remains
`2e3de3a1fec6bffe0d41ca37a670f15a535d2579`.
Only an acceptance script and documentation changed. No bug found or EXE rebuilt.

## Runtime and checks

Used the sealed10,025,472-byte EXE, SHA-256
`04678c6585fad8994a463529540b71051722e81693d9357731a2e82597cea7ac`,
from `mask-luminance-artifact-2e3de3a`. A new runtime copy matched that hash.

The new D-only profile was created by the existing acceptance guard. PID125860,
start `2026-10-22T07:49:54.9890142Z`, executable path and exact process start
 ticks matched. Its own WebView PID98940 command line matched the dedicated
profile and port9493. No Recovery dialog was visible or operated; no normal
production document/profile was loaded.

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

After readback, the existing `finish_close` IPC was invoked once. The page
closed; read-only inspection at07:51:23.9369469Z found PID125860, its known
WebView PID98940, their direct descendants and the9493 listener absent.
No force kill, duplicate close request or OS dialog command was used.

Working evidence lives below:
`D:\Nect-codex-output-20261022-task13-f336c94f2b714cd5bc73d6ca646c8f2e\mask-luminance-native-20261022`.
`native-cases/receipt.json` records six JSON/SVG/Canvas-SVG/PNG cases, numeric
oracles, project readback and close. `evidence/launch.json`, `identity.json`,
`exit.json` and `verification.log` record runtime identity and lifecycle.
The separate `mask-luminance-native-artifact-20261022` seals these small files,
script/docs and a manifest; profile/Recovery/cache directories are excluded.
Existing candidates and examples were preserved.

## D-checkout ownership handoff

The sole active writer is this task's independent remote-backed clone:
`D:\Nect-codex-output-20261022-task13-f336c94f2b714cd5bc73d6ca646c8f2e\mask-luminance-20261022\Nect`.
Branch `codex/mask-luminance` was clean at c5fce804 before this acceptance-only
follow-up. Its origin is https://github.com/46slv/Nect.git, draft PR73 targets
`codex/mask-image-alpha`. The final receipt commit is reported in Git/PR.

This was a new shallow single-branch clone at verified PR72 receipt
`8ab305a33ecf9f8dade2742ec082dbaa2fce0373`, not a moved/shared Git worktree.
The original authorized C checkout `task-13\Nect-pen-baseline` remains on
`codex/mask-image-alpha` at that8ab305a3 commit, clean under read-only inspection
for this handoff. It was not edited. No other checkout was used.

The existing `mask-luminance-20261022/evidence/capacity-at-start.json` recorded
C free0 bytes and D free36,522,962,944 bytes. That prompted the D clone, with
D-local TEMP/TMP, output and Cargo target. Existing installed Node dependencies
and Cargo registry cache were copied read-only from the authorized C task to D;
there was no install, credential/global-config copy, system setting/guard change
or deletion. See that folder's `clone.log`, `dependencies.json` and
`cargo-cache.json`. This handoff did not repeat disk-wide investigation.

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
checks and focused/headless suites were not repeated. No main merge, release,
installer or Library upload occurred. This closes the bounded Mask extension;
no next feature was started.
