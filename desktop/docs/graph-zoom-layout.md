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

## Verified result

Runtime commit `5a283ea4208ffda6616aec466136fac6a2f0ffa9`; EXE SHA256
`bc8f33627a0ed36a49e21fd0c4d389d3c1cc1199444c159f266056965b365a1f`
(10,001,408 bytes). Product changes are confined to main.js Graph scale/scroll
space and eight CSS lines. No core/schema/serialization changes.

Settled `before-stable` versus `after-stable`, same byte-identical 160-node/
158-connection fixture, 12 alternating zoom clicks, same WS/viewport/DPR:

| Measurement                     |   Before |    After |
| ------------------------------- | -------: | -------: |
| Dedicated WebView2 CPU seconds  | 2.125016 | 0.969192 |
| Wall seconds                    | 2.472066 | 1.395970 |
| One-core-equivalent average CPU |   85.96% |   69.43% |
| rAF interval p95                |  108.5ms |   18.9ms |
| CDP click await p95             |  176.3ms |   76.3ms |
| Long tasks                      |       12 |        0 |
| Script seconds                  | 0.096505 | 0.078108 |
| Layout seconds                  | 0.370105 | 0.014504 |
| Style recalculation seconds     | 0.468813 | 0.020907 |
| Wire SVG replacements           |       12 |       12 |
| App-root/Canvas child mutations |      0/0 |      0/0 |

Actual CPU counters come from CDP SystemInfo for the dedicated native WebView
browser/renderer/GPU/services. CPU work falls 54.4% in this fixed operation; this
is one short comparison, not a general speedup claim or whole-host utilization.
rAF cadence and CDP round trips are separate from physical input latency.
Separate diagnostic trace: layout 48 events/341.5ms → 36/24.8ms; style
441.1 → 25.4ms; paint 312.4 → 165.3ms; EventDispatch inclusive
877.8 → 127.9ms. These durations overlap and are not summed as CPU.

The first after timing overlapped a startup/root rerender (app-root mutations 1,
only 2 observed wire replacements). Exclude it. The harness now rejects such a
zoom run and requires all 12 updates on the same SVG. Both accepted timings have
zero root changes and exactly 12 wire replacements. No idle/pan/drag before
matrix was repeated. Minimal after drag: rAF p95 18.8ms, CPU 2.037689s, wall
2.550549s, 39 wire updates, authored JSON unchanged; PR49's existing receipt is
18.8ms/2.074373s/2.532010s. No drag speedup is claimed. First quiet-idle probe
just after trace observed 2.79% one core; a settled idle-only check is 0.025302
CPU seconds/1.594271s (1.59%), zero script/layout/style or DOM writes. These short
idle counters do not establish a sustained CPU-rate change.

Focused DOM: eight existing entries pass (wire lookup, autopan, snapping,
overview), plus the new exact world-wire/anchor/margin/extent/limit/state test.
The initial new fixture lacked native scroll-origin clamping; emulate it and
rerun that case successfully without weakening the assertions. Existing E2E
internal scale probes now read the rendered matrix/transform; syntax checks pass,
but those broad E2E workflows were not rerun.

Native baseline anchor/wire case passes. Candidate 4/4 native cases pass: center
and pointer anchors, all connected cubic endpoints, reset, 5%/200% limits and
scroll extents, middle pan, fractional-scale detached node preview and one
Undo/Redo, measured fit-all/fit-selection, stable selection/JSON/SVG/history.
The first fit probe used DOMRect names for Playwright's x/y box; correct that
fixture and rerun the finite contract checks. Zero page errors. Frontend/native
release builds and native-config/scoped-format/diff checks pass; the existing
bundle-size warning remains. Owned scratch processes close through registered
finish_close without forced termination. Prior PR40–49 candidates remain sealed.

Receipt, raw traces/profiles/summaries/fixtures/screenshots/build/focused logs and
candidate live in `graph-zoom-layout-artifact-5a283ea` beside the task checkout.
Physical input, production documents, image/dither load, other DPI/occlusion,
OS dialogs/cold reopen, whole-host CPU and long stress remain outside this run.
The fixed scene no longer reproduces the previous zoom long tasks. The existing
drag-start 196ms long task/gesture validation remains a separate possible scope;
this change does not claim to resolve it or arbitrary Graph workloads.
