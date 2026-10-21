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

## Native result

Runtime source `5a91b2503db22e18be8026dd7989bdd27d9eb17a`. Release EXE
SHA256 `4ad1296fd3351c3c2d0e680ad6182db32029875089c1205affca6355fe824310`
(10,001,408 bytes). Same WS, fixtures, viewport/DPR and finite operation counts.
One before/after run; these are observations, not broad statistical guarantees.

| Nodes/edges | Operation  | WebView CPU seconds before → after | rAF p95 ms before → after | CDP await p95 ms before → after | Wire DOM writes before/after |
| ----------- | ---------- | ---------------------------------- | ------------------------- | ------------------------------- | ---------------------------- |
| 16/14       | quiet idle | 0.0155 → 0.0170                    | —                         | —                               | 0/0                          |
| 16/14       | pan        | 0.5389 → 0.4944                    | 18.8 → 18.7               | 20.6 → 19.8                     | 0/0                          |
| 16/14       | zoom       | 0.9951 → 0.8650                    | 18.8 → 18.8               | 73.2 → 64.3                     | 12/12                        |
| 16/14       | node drag  | 0.8141 → 0.7437                    | 18.8 → 18.8               | 22.7 → 24.5                     | 39/39                        |
| 160/158     | quiet idle | 0.0195 → 0.0177                    | —                         | —                               | 0/0                          |
| 160/158     | pan        | 0.7269 → 0.7826                    | 19.1 → 19.0               | 20.4 → 19.7                     | 0/0                          |
| 160/158     | zoom       | 2.7125 → 2.3533                    | 126.6 → 108.5             | 198.7 → 175.2                   | 12/12                        |
| 160/158     | node drag  | 2.6187 → 2.0744                    | 37.1 → 18.8               | 63.0 → 42.6                     | 39/39                        |

For medium drag, operation wall time is 3.389 → 2.532 seconds, CPU work falls
20.8%, and script duration 1.615 → 0.856 seconds. Average one-core-equivalent
CPU utilization rises 77.3 → 81.9% because the operation completes sooner; do
not describe that as a utilization reduction. Medium zoom CPU work falls 13.2%,
one-core utilization 101.5 → 95.1%, script duration 0.317 → 0.106 seconds.
Quiet idle stays around 1% of one core with no wire/Canvas DOM writes. Pan has
zero wire draws and no consistent CPU benefit; this change does not target it.
Separate Tauri host CPU deltas over the timing/trace batch are 0.03125 → 0.015625
seconds, excluding startup and additional functional checks.

`drawWires` including descendants across all call sites accounts for 590/2551
profile samples before and 79/2024 after; self samples 310 → 28. Total sample
counts include idle. Trace drag EventDispatch inclusive duration 1793 → 987ms;
layout 109.6 → 103.6ms, style 85.9 → 82.8ms, paint 127.2 → 112.7ms. SVG wire
replacement counts remain unchanged: the fix removes lookup work, not draws.

Native functional checks pass: node preview stays unauthored, release changes
only one node position, one Undo/Redo restores/reapplies it, export is identical,
and disconnect/Undo rebuilds exactly the same 14 wire paths. All eight timing
phases preserve authored JSON, both native runs have zero page errors, and both
owned processes exit through `finish_close` without forced termination.
Frontend/native release builds and native config check pass; only the existing
bundle-size warning remains. PR40–47 sealed EXEs and PR48 receipts are preserved.

Raw summaries, process counters/ancestry, fixtures, profiles, traces, screenshots
and focused/build logs live in the sealed `graph-performance-artifact-5a91b25`
output beside the task checkout. This report deliberately does not claim whole
host CPU, physical mouse/keyboard, user production document, image/dither-heavy
graphs, OS dialogs/cold reopen, other DPI/occlusion conditions or long stress.
The residual medium zoom 108.5ms p95/12 long tasks and node-drag startup long
task remain. The next useful performance scope is trace-backed graph bounds/
style/layout or gesture-validation work under a separately bounded change.
