# Direct-edit-flow Mission

## Mission Brief

**Mission identity:** `Nect/direct-edit-flow`; same primary executor, same `codex/practical-alpha` branch.

**Final Goal:** Make repeated selection, close inspection and small placement adjustments efficient in ordinary desktop production, preserving shared Session authoring semantics.

**Completion conditions:** Scope-aware selection and framing work on real grouped/point artwork without modifying documents; keyboard placement uses existing atomic Session commands with Undo and source preservation. Native format remains0.13. Actual Windows GUI and focused contracts establish the acceptance paths.

**Constraints / Authority:** Current user's continuous-development authorization supersedes old stop/handoff rules. Commit/push only the working branch. No release, main push/merge, dependencies, account changes or unrelated edits. Same Task continues across Mission boundaries. Accepted direct-editing/expert workflow direction in `docs/product-direction.md`; not a blanket Candidate backlog.

**Coarse checkpoint map:** (1) Scope-aware Select All and Fit Selection, including safe keyboard focus. (2) Small keyboard moves through existing object/point Session translations, with predictable world units and Undo. Reassess live production value afterward.

## ACTIVE CHECKPOINT

**Goal:** Select and closely frame artwork in the current editing context without repetitive Shift-click/zoom work.

**Current phase:** Daily-layout accepted (38/38 full regression, actual GUI); synchronize spacing checkpoint, then begin scoped selection.

**Proven:** Alignment synchronized at `811e38b59e27723b68fd41d2538c4f7265a30df9`. Equal gaps have focused core/window/formal MCP acceptance and actual Windows GUI Undo/Redo/readback in `build/daily-layout/gui-spacing.json`. Evidence owner `docs/first-usable.md`. Live Canvas has Fit Artboard/all Artboards, Group scope and point/object selection, but no Select All command or Fit Selection.

**Next task:** Add scoped Select All and Fit Selection using current selection/geometry caches. Check text-field shortcuts remain native; points frame their actual world positions, whole objects frame their evaluated geometry. Keep hidden mask-only objects out of normal Select All, avoid ancestor+descendant selection.

**Approach:** Reuse Canvas view and selection state; no authored state/API command for viewport changes. Explicit View/Edit actions; canvas shortcuts only where they do not capture text editing. Degenerate/empty selections and active gestures must not cause accidental framing or edits.

**Done for next:** Group/Composition scope, point context, fit bounds, no authored revision changes, focus-safe shortcuts and real Windows GUI proven; checkpoint committed/pushed with HEAD equality. Continue same Task to keyboard placement.

**State:** `D:\Documents\Nect`; branch `codex/practical-alpha`; spacing changes awaiting commit. Owned GUI closed. Full access / never approvals.

**Authority:** Current explicit continuous-development instruction and accepted direct editing direction.

**Handoff:** CONTINUE_CURRENT_TASK; no successor/new Task.
