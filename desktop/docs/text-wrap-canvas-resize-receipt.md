# Canvas Text wrap width: verification receipt

Base: PR52 `codex/graph-cancel-render`, remote
`8351a98cc7c00d666b0a5329f40c9b8988add6f9`.
Product/build commit: `9318fd4a688db38928617ea9840ffadf99108e20`.
Branch: `codex/text-wrap-canvas-resize`. Draft targets that verified feature base.

Product delta: `main.js` adds the right-edge overlay and a coalesced Canvas
controller using the existing Session `text-values/wrap` gesture.
`text-wrap-resize.js` resolves the supported Text capsule, logical frame and
inverse affine width. No core/schema/style changes. Existing text, character IDs,
position, connections and selection identity remain intact. The only committed
authored delta in native acceptance was `textWrap.width`.

Supported scope and deferred references are in `text-wrap-canvas-resize.md`.
Source selection retains the existing pre-downstream overlay convention;
whole-artwork mode supports one static positive axis-aligned Transform.

Focused Node model/actual-main DOM: **20 pass, 0 fail** across the new geometry,
gesture and Canvas tests, existing object-resize DOM and Canvas marquee DOM.
Includes clean lifecycle cancel, one Undo/Redo, pending release, no-op, stale
document, guards, numeric bounds, overflow, preserved schema and fixed frame
origin. Four pointer moves coalesce to one Session update/Canvas draw, with zero
full render, wire draw or Graph bounds update during preview.

Two additional old tests were inspected and left unchanged. Both reproduce with
PR52's exact main.js and unchanged core: `numeric-scrub-dom.test.js:119` expects
480 but observes 500; `text-wrap-dom.test.js:82` expects schema 8 but observes 24.
They are pre-existing failures, not counted as passes. The bounded base-source
loader and logs are retained; no full audit or assertion weakening was done.

Native Tauri/WebView2, SHIRO-WS: **10 acceptance cases pass**, no page errors.
Viewport 1469 x 827 CSS px, devicePixelRatio 1.0900000334. UI zoom 100% and 120%
(measured SVG CTM scale 0.9999893875 and 1.1999872424), with Canvas pan and an
additional static Text Transform scale 1.5 in the second case. Commit, one
Undo/Redo, export, JSON reload, Escape, pointercancel, blur, numeric coexistence,
overflow and rotated whole-artwork rejection were observed. Preview preserved
committed JSON/SVG/file/history; cancel also restored visible Canvas and selection
exactly. Preview Graph DOM mutations: zero. Screenshots inspected at both zooms.

The initial native test incorrectly expected an idealized exact width 220 from
synthetic client coordinates. Delivered coordinates yielded 220.0000373839 and
220.0000049822. The final oracle verifies the inverse of actual received pointer
coordinates within 1e-9 document units, separately bounding synthetic input
quantization below 1/1024 CSS px. Exact authored width-only delta and cancel,
Undo/Redo, JSON and SVG assertions remain unchanged. No product rounding added.

Frontend build, native build-config check and locked/offline release native build
pass. Existing Vite chunk-size warning remains. Native process 22900 exited
normally via `finish_close`; no forced termination or existing user process use.
All eleven prior sealed EXE hashes from PR40-52 match their saved receipts;
PR48/51 diagnostic-only receipts remain in their dedicated directories.

Candidate: `task-13/text-wrap-canvas-artifact-9318fd4/nect-svg-desktop.exe`,
10,002,432 bytes, SHA256
`ffca4a59dee775f3763e45ad0ec00430c3f980a15b34b94ee3f5efb3cd77d9f0`.
Its dedicated evidence directory includes logs, native JSON/SVG, screenshots,
build metadata, normal-close proof and prior-artifact hash verification.

Not run: physical mouse/touch/pen, actual OS focus loss, OS save/open dialogs or
cold reopen, IME, unsupported transforms, vertical or multiple-Text resize.
Native inputs were CDP synthetic mouse/keyboard and renderer cancel events.
Clean-file cancellation was verified through the actual-main DOM lifecycle;
native fixtures were unbound/dirty through the supported document API. JSON
reload is not an OS-file reopen claim. No main merge or release.
