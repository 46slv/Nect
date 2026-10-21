# One render for Graph node Escape cancellation

Base: verified PR51 `codex/graph-drag-start`, remote
`85d90676dcaecb97b852fdbc4cc715b8ce95fab5` at
`https://github.com/46slv/Nect.git`. Preserve all prior candidates and receipts.

The confirmed issue is ending Escape EventDispatch 222.657/235.369ms on the same
160 expanded nodes/158 connections, no images/dither/groups. This is not the
start or zoom issue. Existing PR51 trace has two app-root child mutations per
cancel and 135.124/138.763ms nested layout/style. Source reconciliation finds
`cancelInteraction()` renders, then the common Escape handler cleans up more
view state and renders again.

Remove only the intermediate render for Graph-node Escape. Keep Session
cancel-gesture, rollback snapshot/history, pointer release, RAF cleanup and the
final render after all common Escape state cleanup unchanged. Other gestures
and pointercancel/blur/lostcapture retain their existing rendering paths. One
final full render intentionally handles newly selected nodes, named groups,
Inspector/Canvas and current view state; do not introduce a partial renderer or
move the cost to drag start. This is one duplicated-work cause, not a renderer
refactor or history optimization.

Focused acceptance: one render per Escape, exact JSON/SVG/Undo/Redo restoration,
existing/new/multi selection, groups and Inspector/Canvas parity, no-move cancel,
other cancellation routes and later commit/Undo. Same-scene native comparison
uses the retained PR51 two-trial baseline and the bounded stage-marked script;
only cancellation values form the before/after claim. No repeated zoom matrix or
full audit, physical-latency claim, process force-stop, main merge or release.

## Verified result

Runtime source `d599c675604c775a2cf64beea10f20f9b391527c`; release EXE SHA256
`c0309ed732bab381302e1917507ee3959ff2c566c093a01a89edac51afc066d0`
(10,001,408 bytes). Only product delta: three changed main.js lines plus a
comment. No Session/core/schema/history change or snapshot elision.

Native before is the retained PR51 two-trial trace; after uses the same bounded
script, exact serialized scene, WS/viewport/DPR and synthetic CDP input:

| Escape observation       | Selected before → after | Newly selected before → after |
| ------------------------ | ----------------------: | ----------------------------: |
| EventDispatch            |     222.657 → 112.831ms |           235.369 → 118.735ms |
| Nested layout/style      |      135.124 → 65.287ms |            138.763 → 68.889ms |
| App-root child mutations |                   2 → 1 |                         2 → 1 |
| CDP await                |         273.1 → 148.3ms |               277.7 → 153.3ms |

Both after trials still have a long task (112/118ms). This change removes the
duplicate full render; it does not remove the necessary final full render.
These are two EventDispatch observations, not p95, physical/visual input delay
or a general workload speedup. Stage CPU counters remain available separately
(0.422911→0.253342s, 0.316586→0.214077s), including surrounding protocol/profiler/
renderer work, not isolated handler CPU. Nested durations overlap.

Focused: eight existing DOM entries passed (autopan, snapping, named-group
contracts); all five entries of the new cancellation suite pass. It checks
exact authored JSON/SVG/file/history state and Inspector/Canvas/wire UI, fresh
group tickets, existing/new/multi selection, no-move Escape, pointercancel/blur,
preserved Redo snapshot and later one-Undo/Redo commit. The exact baseline source
fails the new one-render assertions with 2 versus 1 (three cases; history case
already passes). No existing assertions changed, removed or skipped.

Fixture corrections were limited to declaring the required schema44 for groups
and treating render-generation tickets as transient while explicitly asserting
freshness. A PowerShell text export corrupted the first baseline probe; binary
git-show extraction verifies the intended 2-versus-1 failure. These diagnostic
logs are retained, not counted as product failures or accepted baseline evidence.

Native assertions pass for both 160-node trials (unchanged preview JSON and
exact cancel JSON/SVG/Undo), and two additional 16-node group/UI cases. Same-
selection Inspector/Canvas/group HTML matches exactly, with group-ticket
freshness verified. Newly selected final UI matches a click-only selection.
All live cubic wire endpoints match their sockets; max observed error0px with
0.001px checking (the existing native contract permits0.1px). The first new
probe compared raw wire strings and saw 0.000061px DOMRect float jitter; the
live endpoint/topology check avoids treating serialization noise as geometry.
Authored SVG and model/UI assertions remain exact. Zero page errors. One 500ms
idle check has no DOM changes, one zoom pair preserves JSON/SVG/selection/
Undo/Redo, and the measured second moves provide the minimal continued-drag
check. No start/zoom optimization or repeated full matrix.

Frontend/native release build, native config, scoped format/diff checks pass;
only the existing bundle-size warning remains. Owned scratch closes normally
with finish_close, no forced termination. Prior ten EXE hashes and PR48/51
receipts remain preserved. Candidate and raw evidence are sealed beside the
checkout in `graph-cancel-render-artifact-d599c67`.

Unverified: physical input, production/image/dither scenes, other DPI/occlusion,
OS dialogs/cold reopen, sustained or whole-host CPU, long stress/full audit.
Remaining scope is the one final full render (~113–119ms here); a future partial
renderer needs its own UI/state contract. No current blocker.
