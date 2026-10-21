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
