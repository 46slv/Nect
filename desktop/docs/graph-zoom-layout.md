# Native graph zoom layout maintenance

Base: verified PR49 `codex/native-graph-performance` at
`b90b9837a1e2ea9208b4c62e06be89113ef8456f`, origin
`https://github.com/46slv/Nect.git`. Preserve its sealed native candidate and
all earlier candidates. Single writer, one reproduced cause only.

PR49's 160 expanded nodes/158 connections, no image/dither/groups, retain zoom
rAF interval p95 108.5ms across 12 alternating toolbar zoom clicks. Script
duration is 0.106s, layout 0.370s, style 0.454s. The retained diagnostic trace is
node drag, not zoom; do not mislabel it. Reuse that evidence and fixture, then
capture only missing zoom timing/profile/timeline in the same native viewport
(1469x827 CSS px, DPR 1.09; Graph 1132x201). Do not repeat the whole prior matrix.

Distinguish actual dedicated WebView process CPU seconds, rAF intervals, CDP
round trips, engine layout/style duration and authored commits. Follow forced
layout initiators before choosing a product change. A justified patch must
preserve wire geometry, graph viewpoint/zoom anchors, selection, Undo, JSON and
SVG output. No virtualizer/engine replacement/new feature. Compare zoom only;
idle and drag get a minimal after non-regression check. Native input is synthetic
CDP in actual Tauri/WebView2; no physical/OS acceptance claims, forced process
termination, long stress, production files or dither workload expansion.

The zoom-only trace reproduces 48 layout events/12 clicks, including a full
10,125-dirty-object layout and style update for 8,622 elements each time. A
temporary getter probe identifies `updateGraphBounds`' first viewport.clientWidth
read after setting inherited CSS zoom as the synchronous 60.6/66.7ms wait.
This is not a document gesture/commit or repeated field search. The main native
profile hotspot is the bounds function, where the browser flush occurs.

Bounded change: keep the existing DOM graph and world coordinates, scale its
rendered contents with a transform, and give a single outer graph-space the
explicit scaled scroll extents. Retain the zoomed 16px margin on every side.
All render, zoom and fit paths already call updateGraphBounds; set scale and
space dimensions there. No persistent geometry cache, deferred commit, extra
draw frequency or virtualization. Existing numeric zoom limits, anchoredScroll,
graphPoint and wire calculation stay unchanged. Internal test probes move from
CSS zoom to the rendered transform/matrix; their existing assertions remain.

A two-click temporary transform diagnostic (not accepted navigation behavior)
reduces layout to 2.26ms and style to 3.46ms, supporting the cause. Full native
anchor/scroll/wire/fit/drag contracts must pass the integrated candidate before
publishing any performance claim. The first zoom probe captured a detached
viewport handle (0x0); exclude its timing. Use the settled before-stable receipt,
and query/assert the current connected viewport atomically in the harness.
