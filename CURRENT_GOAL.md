# Stroke-authoring Mission

## Mission Brief

**Mission identity:** `Nect/stroke-authoring`; same primary executor and Task.

**Final Goal:** Make common rounded, square-ended and beveled line artwork editable and consistent across Canvas/PNG/SVG, without changing source geometry.

**Completion conditions:** Explicit retained cap/join/miter contract with compatible versioning; shared commands/API/MCP, Inspector, real Windows rendering and SVG intake/output. Undo/native/recovery preserve style; unsupported cases remain explicit.

**Constraints / Authority:** Continuous-development instruction, accepted ordered Fill/Stroke product direction and interoperable2D workflow. Existing Qt/core stack; no dependencies, release/main merge or successor Task. Preserve old documents' exact default behavior; avoid broad paint framework/native redesign.

**Coarse checkpoint map:** (1) Establish bounded versioned stroke-style contract, core/native/renderer/export and focused evidence. (2) Inspector and SVG import workflow, actual Windows/persistence acceptance. Distant dash/pressure/variable-width/brush work is outside this Mission.

## ACTIVE CHECKPOINT

**Goal:** Define and implement retained butt/round/square caps, miter/round/bevel joins and miter limit consistently in existing stroke owners.

**Current phase:** Core/native/API/MCP/Canvas/PNG/SVG checkpoint implemented and accepted; preparing coherent checkpoint commit and synchronization.

**Proven:** `docs/first-usable.md` records Release build, repaired stroke/MCP2/2 and full40/40 regression (34.43s). Native/Undo/source preservation, linked miter, invalid atomic commands, SVG attributes and PNG pixel probes pass. Initial legacy-alias failure and repair evidence retained in `build/daily-layout/stroke-*.log`. Previous selection-transform and responsive-editing evidence remains there.

**Next task:** Save and synchronize this checkpoint; record the next Inspector/SVG intake checkpoint as unstarted, then stop per latest user instruction.

**Approach:** Retained Stroke behavior v2 within native0.13, explicit promotion only; v1 default encoding unchanged. Qt SVG miter semantics and derived zero-length caps. Next checkpoint must use this same Session command and shared paint without duplicating core logic.

**Done for next:** Core/renderer/export acceptance complete. Coherent commit and remote synchronization remain before stopping. GUI/import/visible Windows acceptance belongs to the next Task.

**State:** `D:\Documents\Nect`, `codex/practical-alpha`; base `0d0b5f40037b360c8c5f178beeb8ec6de97f6b7b`, accepted Stroke changes ready to commit. No owned GUI or test/build running. Full access/never approvals; native0.13.

**Authority:** Explicit continuous accepted-direction development and bounded lightweight research; working-branch commit/push allowed. Latest user instruction overrides continuation after this checkpoint: save, synchronize, report and provide a next-Task prompt, then stop.

**Handoff:** STOP_AFTER_CURRENT_CHECKPOINT. Latest user instruction requests stopping after this checkpoint and returning both a report and a successor prompt. Do not create or start a successor automatically.
