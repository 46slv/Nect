# Stroke-authoring Mission

## Mission Brief

**Mission identity:** `Nect/stroke-authoring`; checkpoint1 complete, checkpoint2 (NECT-STROKE-CP2 r4) resumed from the fixture-access block.

**Final Goal:** Make common rounded, square-ended and beveled line artwork editable and consistent across Canvas/PNG/SVG, without changing source geometry.

**Completion conditions:** Explicit retained cap/join/miter contract with compatible versioning; shared commands/API/MCP, Inspector, real Windows rendering and SVG intake/output. Undo/native/recovery preserve style; unsupported cases remain explicit.

**Constraints / Authority:** Continuous-development instruction, accepted ordered Fill/Stroke product direction and interoperable2D workflow. Existing Qt/core stack; no dependencies, release/main merge or successor Task. Preserve old documents' exact default behavior; avoid broad paint framework/native redesign.

**Coarse checkpoint map:** (1) Establish bounded versioned stroke-style contract, core/native/renderer/export and focused evidence. (2) Inspector and SVG import workflow, actual Windows/persistence acceptance. Distant dash/pressure/variable-width/brush work is outside this Mission.

## ACTIVE CHECKPOINT

**Goal:** Complete practical Stroke style authoring through Inspector and strict SVG import, sharing existing StrokeStyle/core paint semantics.

**Current phase:** S0_COMPLETE → S1_COMPLETE → S2_COMPLETE → S3_COMPLETE / `REMOTE_SYNC_PENDING`. Canonical Brief r4 is `READY`; the exact inline worker-access patch has resolved `BLOCKED_FIXTURE_ACCESS`.

**Proven:** Canonical r4/READY was re-fetched. Exact inline fixtures are materialized under ignored `build/stroke-cp2-r4-fixtures/` and rechecked after testing: r1 5597 bytes / SHA-256 `4b23b6f719a335155164786ff3738d277bd9e5ad2908e1b6870a33fa328dd1c1`, r2 5877 bytes / SHA-256 `fc586ecb77076b50d23933bf185ee679f409bea87503017c59ceac166e4e65ee`; both are UTF-8, BOM-free, LF-only, and end with LF. S0 build/Qt deployment succeeded; changed-code focused Release is 7/7 and full Release CTest is 40/40. T1–T7 pass through the Inspector, strict SVG fixture/session/native/Undo and regression contracts; T8 visible before/after benchmark evidence is in `build/stroke-cp2-r4-benchmark-before-20260923-102733/` and `build/stroke-cp2-r4-benchmark-after-20260923-110840/` (same Windows/Qt, 2/80 paths, 1440x900, warm-up12, 90 measured frames; 30fps p95 and 33ms release floors pass in both; the 60fps aggregate is false in both and is not a CP2 failure). Historical S0 focused baseline 5/5 remains separate. Pre-implementation baseline HEAD was `083b53d520634abb797973612e640ec344c9384c`.

**Next task:** None in this packet. Attempt working-branch push/readback once, record the exact result, then stop without advancing to the next feature.

**Approach:** Inspector exposes butt/round/square caps, miter/round/bevel joins and miter [1,1000] using StrokeStyle. Preserve v1 unless explicit style promotion is needed, and preserve unchanged driven miter. Extend SVG presentation/inheritance parsing only for supported styles; retain explicit rejection of dashes and unsupported values. Add focused adapter/import tests and a representative fixture, then visible Windows native/Undo/Redo/recovery/export acceptance. Do not reproduce core stroke geometry in adapters.

**Done for next:** S1 Inspector, S2 SVG intake and S3/T1–T8 evidence are implemented and verified against the r4 contract. Create one coherent local checkpoint commit; remote synchronization is attempted separately and a DNS failure is reported as `WAIT_REMOTE_SYNC` without discarding the local checkpoint. Do not advance to the next feature.

**State:** `D:\Documents\Nect`, `codex/practical-alpha`, pre-commit HEAD `083b53d520634abb797973612e640ec344c9384c`; seven tracked packet paths are modified (`CURRENT_GOAL.md`, two docs, two desktop sources, two test files). r4 build artifacts and exact fixture scratch are ignored; no owned GUI/test process remains. Remote DNS availability is independent of local implementation.

**Authority:** User-started NECT-STROKE-CP2 r4 / READY permits bounded implementation, tests, docs and working-branch synchronization. The exact inline r1/r2 bytes are now verified, so the fixture stop condition is cleared. No main merge, release, distribution, policy/account changes, paid dependencies, shared-history rewrite or next feature.

**Handoff:** `CONTINUE_CURRENT_TASK`. Owner documents: AGENTS.md, START_HERE.md, docs/model-v0.md, docs/svg-import.md, docs/quality.md and docs/first-usable.md. Stop after this packet; do not start the next feature.
