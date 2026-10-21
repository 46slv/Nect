# 2D Repeater Alternate Scale: verification receipt

Base: verified PR57 remote `c67b5647f4f5a80c4d7ea6a5888ca46a12b2a04f`,
`codex/text-wrap-rotation-resize` in `46slv/Nect`.
Branch: `codex/repeater-alternate-scale`.
Product/build source: `9ae030d065f4d2c109b47fc3a1ace23f8946857a`.

The `ae-2d` Geometry Repeater exposes one existing Scalar-style control,
Alternate Scale (%), default100, finite range0.01–1000. Zero-based copies1,3,5…
receive the uniform scale around the source Anchor before their existing affine
placement. The accumulated matrix advances with the original step only. Copy
index is independent of Offset and reversed paint order. Point positions and
relative handles share the effective transform; copied Anchor positions, stable
IDs/groups, source, step/rotation, opacity, partial tail and order are preserved.
Legacy Linear/Radial modes retain their prior controls/geometry. The linear and
radial seeds are two affine configurations of `ae-2d`, not new operations.

Old documents backfill100 without promotion. Authored variation/expression/wire
requires schema45; the shared Session promotes before validation/history. A
schema45 document requires the descriptor. Older retagged nonidentity documents
reject. No Color/dither/text/layout change, index language or painted duplication.

Focused model/Session: **15 pass, 0 fail, 0 skipped** across
`repeater-alternate-scale.test.js` and the existing9 `repeater-ae.test.js` checks.
Two24-copy seeds check odd scales60/150, exact even identity, positions/handles
around copied anchors, source and IDs. Negative/fractional offsets, nonuniform
step scale, reversed order, holes/open state and fractional tail opacity retain
their contracts. Boundary0.01/1000 is accepted; zero/negative/nonfinite/out-of-range
literal/expression and invalid driven values reject without history/document
mutation. Locks, retained value expression, one edit/Undo/Redo, first-drag schema
preview/cancel/commit, JSON/SVG and version/backfill/retag checks pass. This run
also preserves the existing schema36 Dither migration assertion without rerunning
unrelated dither suites.

Actual PR57 `repeater.js` loaded from Git and the new100% implementation produce
**exactly equal geometry in10 comparisons**: linear/radial24-copy fixtures with
offset-1.5/-1/0/0.5/1, including handles, IDs/groups and opacity. No tolerance was
introduced for default or even-copy equality.

Native Tauri/WebView2 on SHIRO-WS: **2 configurations, 8 acceptance checks pass**.
Four-node/three-connection Curve→Repeater→Fill→Output scenes,24 copies, no images
or dither. Linear anchor(100,270), position(32,0), offset-1; radial anchor(480,120),
rotation15deg, position(38.8228567654,5.1111260566), offset1. Both change100→60 via
the actual Inspector. Exact document comparison admits only this value/schema;
copy geometry/handles/identity, unchanged source/settings and single Undo/Redo
pass. Scrub Escape plus late pointerup restores exact document/SVG/file/history/
selection; zero UI input rejects; JSON reload restores exact authored data/SVG.
Viewport1469x827 CSS px, DPR1.0900000334, Fit artboard approximately58%. Both
native screenshots inspected. Page errors0. PID104648 exited normally via guarded
finish_close; no forced process termination.

One representative headless Chrome production-CSP check exercises the **existing
PNG export/download UI**, without OS dialogs. The radial24-copy60% result has
960x540 RGBA digest
`3df36ad250da11dff01dede14fe15855584852ed6a3133d2a2a67070b35969cd`
for PNG, exported SVG and Canvas shapes, all exact. This is browser PNG evidence,
not native OS-save evidence.

Frontend/config, syntax/format and locked/offline native release build pass.
Existing Vite chunk-size warning remains. Before build, all16 prior sealed EXE
hashes and complete PR55/56/57 manifests (104 evidence files) match. Earlier
candidates are preserved in separate directories.

Sealed candidate: `task-13/repeater-alternate-scale-artifact-9ae030d/nect-svg-desktop.exe`,
10,002,944 bytes, SHA256
`81373969fb7596bb1e7bf303090059c04b6303c9a53a69a708c7197a97a3f653`.
JS `index-CRSXO7z3.js`, SHA256
`962b3dac1b11a86f42443ed2fca36c351da8a94ae7c3701786d9d840470d52fc`.

Not run: physical input, actual OS focus loss, OS save/open/cold reopen or native
PNG save dialog, AE runtime comparison, unrelated full suites/performance audits.
Native input is CDP synthetic; fixtures unbound/dirty. JSON reload does not prove
OS-file reopen. Copy-specific color variation remains a concrete unmet feature:
paint is still shared after geometry, with no new per-index language here. No
protected-main merge, release or Library upload.
