# Rotated Text wrap-width handle: verification receipt

Base: PR56 `codex/text-wrap-cross-reference-resize`, verified remote
`04b5b802002437999de763e7652dee16f63efb25`.
Product/build commit: `d24acc9f6e950638794e1d0657c61e796035b2bd`.
Branch: `codex/text-wrap-rotation-resize`.

The existing horizontal auto-height Text whole-artwork capsule now accepts
rotation through its one static Transform. The helper uses the same evaluated
`transformValues().matrix` as the renderer; logical frame corners and the handle
are mapped from node-local coordinates. The existing Canvas CTM inverse and full
affine delta inverse recover local X for width. Pivot, Position/Offset, X/Y
references and zoom/pan are each applied once. Only the cursor uses the displayed
axis angle. Zero-degree behavior, positive independent X/Y scales and source-mode
pre-downstream coordinates remain. Skew, reflection, collapsed/ill-conditioned
matrices and unsupported routes stay hidden. Finite frame points are required.
No core/schema/layout change, authored transform adjustment or general resize.

Focused Node model/actual-main DOM: **28 pass, 0 fail, 0 skipped**. Model cases
cover 0/90/-45/37 degrees, nonorigin Transform anchor/Position, local-Y no-op,
90-degree world-X no-op/world-Y width, bounds, numeric-command JSON/SVG equality,
identity/transform preservation, cancellation and invalid matrix/delta guards.
Three rotated DOM cases combine center/end X/Y references, zoom/pan, shrink/grow,
orthogonal movement, one Undo/Redo, numeric-control equivalence and Escape,
pointercancel/blur with late release. Existing zero-degree and lifecycle tests
remain in this focused run; unrelated historical suites were not rerun.

Native Tauri/WebView2 on SHIRO-WS: **2 configurations, 12 acceptance checks pass**.
90 degrees with X/Y Center at100%, and -33 degrees with X/Y End at120%; both use
scale1.5 and nonorigin anchor/Position. Four commits near widths200/640 match
numeric edits in exact JSON/SVG, preserve Position/rotation/characters/selection,
have one Undo and restore through Undo/Redo and JSON reload. Six cancel checks
restore exact committed/visible/file/history state. Numeric overflow/export and
skew rejection complete12. Preview Graph DOM mutations0, page errors0.

Viewport1469x827 CSS px, DPR1.0900000334; actual SVG CTM scales0.9999893875 and
1.1999872424 with pan. The input oracle uses received pointer coordinates and
the full inverse matrix, document tolerance1e-9 and a separate1/1024 CSS-px
synthetic quantization bound. Node-local reference remains(140,160). Receipt
retains matrices, pointer traces, JSON/SVG and screenshots; representative
rotated frames/handles were visually inspected. Native PID141972 exited normally
via the registered finish_close command, guarded to the task-owned unbound
fixture with no live gesture; no force termination.

Frontend/config and locked/offline release native build pass; the existing Vite
chunk-size warning remains. All15 prior sealed EXE hashes matched before build.
PR55's complete30-file and PR56's33-file manifests also match, retaining the
numeric Escape repair and migration evidence without repeating those suites.

Candidate: `task-13/text-wrap-rotation-artifact-d24acc9/nect-svg-desktop.exe`,
10,002,432 bytes, SHA256
`c9c0c927d067e8774226ec1b11e0b731b070f8ea525916fc27d5d1af82b59ba5`.
JS `index-Bdxqc6qb.js`, SHA256
`0c10f410433c9dba38558dbf866970312825d3d3e8045e2c83e5318dd17d6010`.

Not run: physical mouse/touch/pen, actual OS focus loss, OS save/open/cold reopen,
IME, every angle/reference combination in native, unsupported routes/fonts,
skew/reflection, multiple Text or generic artwork resize, unrelated rendering,
dither/performance/full audits. Native input is CDP synthetic mouse/keyboard and
renderer cancel events; fixtures are unbound/dirty. DOM covers clean lifecycle;
JSON reload is not an OS-file reopen. Skew and other general transform routes
remain concrete unmet capabilities, outside this rotation-only slice. No main
merge, release or Library upload.
