# Native graph performance maintenance

Base: PR48 `cb79d02195b8bfe503968afe98961b31e34e0c1c`, branch
`codex/selection-suite-maintenance`, remote `https://github.com/46slv/Nect.git`.
The user reports a heavy node graph and requests CPU/interaction testing. Scope
is one trace-supported bottleneck in current Graph operation, with no new feature,
panel/refactor or altered selection/model/output contract.

Existing `graph-autopan-work.md` already verifies that stationary/clamped held
node/wire callbacks avoid preview/draw work. Its explicit limits exclude live
native frame-time/CPU evidence. Do not repeat that solved case. Current PR48 is
test-only; before measurements use the unchanged PR47 runtime source
`8236aebdfecd4ec8dd5346d9d1ed9bcfab64b942`, EXE SHA256
`09aa0d9170fd1a0817b52380b983198460744ce08e43b64cabdb2bef933c3d2e`.
Use an identical EXE copy in a new task-owned acceptance directory, preserving
all sealed candidates. Do not operate/stop unrelated user processes.

Prepare deterministic small/medium static Graph scenes with recorded node/edge
counts, expanded/collapsed state, image/dither presence and viewport. Observe short
finite idle, pan, zoom and node-drag sequences. Distinguish measured process CPU,
rAF cadence/CDP operation latency, DOM mutation counts and native trace/layout/
paint/profile evidence. CPU is unavailable unless an actual counter succeeds;
frame times are not a CPU substitute. State occlusion/visibility and synthetic
input limits. Cancel authored drag previews and close only the owned scratch
through the normal native close path; no process force-stop or prolonged stress.

First measure, then select at most one reproduced principal cause. A product
change requires same-condition before/after evidence, focused semantic checks,
native release build and target native verification. If no convincing degradation
appears, preserve the measurement/report without inventing an optimization.

## Reproduced cause and bounded change

On SHIRO-WS (24 logical processors), the expanded 160-node/158-connection
fixture reproduced slow zoom and node drag. The native CPU profile attributes
590/2551 samples (including 790 idle samples in the denominator) to `drawWires`
and descendants. Every connected parameter obtains all `.field` elements and
linearly searches them again. The 16-node regression fixture proves 14 complete
field traversals per draw before the change.

Build a lazy, draw-local node-ID/parameter-key map once instead. Keep the first
matching rendered field, discover fresh fields next draw, and leave socket
geometry, wire markup, command validation, rendering cadence and model/history
unchanged. No persistent cache or extra invalidation rules. Unwired graphs and
source-only wire previews still need no target scan. Other profile costs (graph
validation and layout) are outside this change.

## Measurement protocol

`scripts/measure-native-graph.mjs` uses the task-owned native Tauri/WebView2 CDP
endpoint and `tests/fixtures/graph-performance.js`: 16 nodes/14 connections and
160 nodes/158 connections, expanded, six columns, no images, dither or groups.
Window viewport is 1469x827 CSS px, DPR 1.090000033378601, Graph scroll viewport
1132x201. The page reports visible; input is synthetic CDP, not OS/physical input.
Each scene runs 1.5s quiet idle, 40 finite middle-button pan moves, 12 alternating
zoom clicks, and 40 finite node-drag moves followed by Escape. Authored JSON must
remain identical after every phase. One separate medium drag records a 1ms V8
CPU profile and Chromium timeline, outside the timed phases.

CPU is the actual cumulative process `cpuTime` from CDP `SystemInfo`, summed
across this dedicated WebView2 browser, renderer, GPU and service processes.
`100 * CPU seconds / wall seconds` is one-core-equivalent utilization; it is not
whole-host CPU utilization or physical input latency. Tauri host process CPU is
recorded separately with `TotalProcessorTime`. rAF interval and CDP await p95
measure cadence and automation round trip; neither substitutes for CPU. Wire
mutation counts measure SVG DOM replacements, not paints. Trace durations can
overlap and are not summed as CPU.

The initial idle observer scheduled its own rAF callbacks; those idle results
are excluded. `before-quiet-idle` repeats only quiet idle without that observer:
0.015495 CPU seconds/1.591942s (16 nodes) and 0.019456/1.590822s (160 nodes),
approximately 0.97% and 1.22% of one core, with zero observed DOM writes.
Interactive baseline measurements remain valid. The after script disables the
rAF observer for idle by default. No long stress, unrelated process operation
or process force-stop. The task-owned unbound scratch closes through the
registered native `finish_close` application command.

Focused regression: new actual-main DOM test fails on the baseline 14-versus-1
scan bound, then passes after the change. It preserves exact wire paths,
first-duplicate semantics, next-draw refresh, disconnect/Undo, unwired/preview
behavior and authored state. Six focused test files pass 22 entries, including
existing autopan, snapping, overview, groups and Canvas marquee contracts.
