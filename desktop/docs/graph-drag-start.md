# Drag-start trace reconciliation

Base: PR50 `codex/graph-zoom-layout`, verified remote
`a2ae1b53401cba511686d0bfa8749fc7f54c1338` at
`https://github.com/46slv/Nect.git`. Preserve all sealed candidates. Scope is the
reported 196ms drag-start long task, not another zoom comparison or full audit.

Existing PR49 `measurements/after/medium-drag.trace.json` separates the events:
Control keydown 1.330ms, pointerdown 19.986ms, first effective pointermove
21.435ms, and the ending keydown 204.534ms. Its 201.789ms handler and 117.195ms
layout/style occur at the end, matching Escape in the measured operation. The
previous phase-level long-task aggregate did not retain stage attribution; calling
it a start task was incorrect. Correct the classification before optimizing.

Use the unchanged sealed PR50 native EXE, same 160 expanded nodes/158 connections,
no images/dither/groups, viewport1469x827/DPR1.09/Graph1132x201. Add only the
missing stage-labelled short drag trace: already-selected and newly-selected
node, pointerdown, first moves, Escape. Record actual handler durations, timeline
layout/style, sampled CPU stacks and CDP round trips separately. Ensure startup
renders do not overlap start measurements. Preserve authored JSON, positions,
IDs, wires, SVG and Undo. Keep history snapshots and guards intact.

Only a reproduced, clear start bottleneck warrants a minimal product patch.
Otherwise close this bounded investigation with accurate evidence and a separately
identified cancellation condition. No renderer rewrite, new feature, broad old
suite, zoom matrix, process force-stop or physical-latency claim.

## Result: classification corrected, product unchanged

The exact PR50 native EXE (`bc8f33627a0ed36a49e21fd0c4d389d3c1cc1199444c159f266056965b365a1f`,
runtime source `5a283ea4208ffda6616aec466136fac6a2f0ffa9`) confirms the old trace.
Two short gestures on the same fixed scene, first on the selected Value and
second on another Value, with labelled down/move/Escape stages:

| Native event handler duration | Already selected | Newly selected |
| ----------------------------- | ---------------: | -------------: |
| pointerdown                   |         18.657ms |       19.429ms |
| first effective pointermove   |         22.952ms |       23.812ms |
| second pointermove            |         19.735ms |       19.794ms |
| ending Escape keydown         |        222.657ms |      235.369ms |
| layout/style inside Escape    |        135.124ms |      138.763ms |

Only Escape produces the two long-task entries (222ms, 235ms). Start/move stages
have zero app-root child mutations; Escape has two per gesture and invokes
`cancelInteraction` → cancel Session gesture → full `render()`. The existing
native profile attributes major cancellation work to bounds/layout and rendering;
pointerdown contains Session validation/snapshot preparation but no corresponding
200ms task. History rollback snapshots are intact. A ~196ms _start_ bottleneck
does not reproduce in this bounded condition, so there is no justified start
optimization or product patch. The confirmed residual is full cancellation
rendering, a different maintenance scope. These two trials do not establish
physical/visual latency, worst-case bounds or production-document performance.

CDP round trips are separately 30.8/27.9ms for down, 32.9/67.9ms for first move,
33.8/44.5ms for second move, and 273.1/277.7ms for Escape. They are not handler
duration or hardware input latency. Stage CPU data in the raw receipt are actual
dedicated WebView2 process-counter deltas but include protocol/profiler/renderer
work around each stage, not isolated handler CPU. EventDispatch durations and
timeline layout/style are the primary attribution. V8 samples, unfiltered raw
trace and a compact stage comparison are retained; nested durations overlap.

Both previews leave authored JSON unchanged; both cancels restore exact JSON/SVG
and original Undo availability, all 158 wire paths remain present. Node IDs,
positions and source connections are contained in the exact restored JSON.
There are zero page errors. One additional 500ms quiet check has zero DOM
mutations/layout/style (72µs observed script); one zoom-in/out pair preserves
JSON/SVG/selection/Undo/Redo and wire count. The second-move samples above are the
minimal ongoing-drag check. No zoom matrix/full prior measurements repeated.

No `src`/`src-tauri` delta, new build, broad product suite or redundant candidate.
Only this brief and a reusable bounded diagnostic script are committed. Script
syntax/format, actual native assertions, trace-mark classification and exact
fixture comparison pass. The script stops profiler/trace on failed assertions
and records only input dispatches in compact summaries; full events stay in the
raw trace. Physical input, actual documents/dither, other DPI/occlusion, OS
dialogs/cold reopen, sustained/whole-host CPU and long stress remain unverified.
Prior PR50 product and acceptance results are retained unchanged, with their old
"drag-start 196ms" label superseded by this receipt.

The separate scratch closes through registered `finish_close`, no forced stop.
PR40–50 saved candidates and PR48 receipts remain intact. Evidence is sealed in
the task's `graph-drag-start-artifact-*` directory; reuse the existing PR50 EXE
from `graph-zoom-layout-artifact-5a283ea` for delivery. No current blocker.
