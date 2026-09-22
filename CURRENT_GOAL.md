# Stroke-authoring Mission

## Mission Brief

**Mission identity:** `Nect/stroke-authoring`; checkpoint1 complete, checkpoint2 awaiting the next user-started Task.

**Final Goal:** Make common rounded, square-ended and beveled line artwork editable and consistent across Canvas/PNG/SVG, without changing source geometry.

**Completion conditions:** Explicit retained cap/join/miter contract with compatible versioning; shared commands/API/MCP, Inspector, real Windows rendering and SVG intake/output. Undo/native/recovery preserve style; unsupported cases remain explicit.

**Constraints / Authority:** Continuous-development instruction, accepted ordered Fill/Stroke product direction and interoperable2D workflow. Existing Qt/core stack; no dependencies, release/main merge or successor Task. Preserve old documents' exact default behavior; avoid broad paint framework/native redesign.

**Coarse checkpoint map:** (1) Establish bounded versioned stroke-style contract, core/native/renderer/export and focused evidence. (2) Inspector and SVG import workflow, actual Windows/persistence acceptance. Distant dash/pressure/variable-width/brush work is outside this Mission.

## ACTIVE CHECKPOINT

**Goal:** Complete practical Stroke style authoring through Inspector and strict SVG import, sharing existing StrokeStyle/core paint semantics.

**Current phase:** NOT_STARTED. Prior core/renderer/export checkpoint synchronized at `e49eb8f784743164e61bda8edb38392e52f8e4d8`; local/remote equality verified. User requested stopping at that boundary and receiving a report plus next-Task prompt.

**Proven:** `docs/first-usable.md` latest Stroke entry records Release build, stroke/MCP2/2 and full40/40 regression (34.43s). Native/Undo/source preservation, driven miter, atomic failures, SVG style attributes and PNG pixel probes pass. Initial legacy-alias rejection and repair retained in `build/daily-layout/stroke-*.log`. No new visible Windows or performance claim.

**Next task:** In the next explicitly started Task, verify actual Full access/never-approval runtime and live Git state, then inspect `src/desktop/window.cpp` and `src/desktop/svg_import.cpp` for bounded style adapters.

**Approach:** Inspector exposes butt/round/square caps, miter/round/bevel joins and miter [1,1000] using StrokeStyle. Preserve v1 unless explicit style promotion is needed, and preserve unchanged driven miter. Extend SVG presentation/inheritance parsing only for supported styles; retain explicit rejection of dashes and unsupported values. Add focused adapter/import tests and a representative fixture, then visible Windows native/Undo/Redo/recovery/export acceptance. Do not reproduce core stroke geometry in adapters.

**Done for next:** Styles can be authored and imported, edited non-destructively, saved/reopened/recovered and exported consistently; API/MCP shares the Session path. Real Windows interaction evidence and proportional regression recorded, coherent working-branch commit/push with local/remote SHA equality.

**State:** `D:\Documents\Nect`, `codex/practical-alpha`; implementation baseline `e49eb8f784743164e61bda8edb38392e52f8e4d8`; this handoff is a documentation-only follow-up. Native0.13, Stroke v1/v2. No owned GUI/build/tests running. Current runtime Full access/approval never; successor must verify its actual runtime. Resolve latest handoff HEAD from Git.

**Authority:** Original accepted-direction development permits ordinary reversible implementation and working-branch commit/push. Latest user instruction requires this Task to stop after checkpoint1, with report and prompt. No automatic successor creation, next-checkpoint implementation, main merge/push, release, distribution, policy/account changes, paid dependencies or shared-history rewrite.

**Handoff:** READY_FOR_USER_STARTED_TASK. Current Task stops here; the next Task should use this Mission Brief, active checkpoint and live repository, not old raw transcripts. Owner documents: AGENTS.md, START_HERE.md, docs/model-v0.md (Stroke behavior v2), docs/svg-import.md, docs/quality.md and latest relevant docs/first-usable.md evidence. No long-running worker remains assigned to this checkpoint.
