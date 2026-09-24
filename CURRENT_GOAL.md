# Stroke-authoring Mission

## Mission Brief

**Mission identity:** `Nect/stroke-authoring`; checkpoint1 complete, checkpoint2 (NECT-STROKE-CP2 r4) resumed from the fixture-access block.

**Final Goal:** Make common rounded, square-ended and beveled line artwork editable and consistent across Canvas/PNG/SVG, without changing source geometry.

**Completion conditions:** Explicit retained cap/join/miter contract with compatible versioning; shared commands/API/MCP, Inspector, real Windows rendering and SVG intake/output. Undo/native/recovery preserve style; unsupported cases remain explicit.

**Constraints / Authority:** Continuous-development instruction, accepted ordered Fill/Stroke product direction and interoperable2D workflow. Existing Qt/core stack; no dependencies, release/main merge or successor Task. Preserve old documents' exact default behavior; avoid broad paint framework/native redesign.

**Coarse checkpoint map:** (1) Establish bounded versioned stroke-style contract, core/native/renderer/export and focused evidence. (2) Inspector and SVG import workflow, actual Windows/persistence acceptance. Distant dash/pressure/variable-width/brush work is outside this Mission.

## ACTIVE CHECKPOINT

**Goal:** Complete practical Stroke style authoring through Inspector and strict SVG import, sharing existing StrokeStyle/core paint semantics.

**Current phase:** MUSE2-R1..R5 bounded repair verified locally / `LOCAL_COMMIT_READY`. Canonical Brief r4 is `READY`; the exact inline worker-access patch has resolved `BLOCKED_FIXTURE_ACCESS`.

**Proven:** Canonical r4/READY was re-fetched. Exact inline fixtures are materialized under ignored `build/stroke-cp2-r4-fixtures/` and revalidated by the test loader: r1 5597 bytes / SHA-256 `4b23b6f719a335155164786ff3738d277bd9e5ad2908e1b6870a33fa328dd1c1`, r2 5877 bytes / SHA-256 `fc586ecb77076b50d23933bf185ee679f409bea87503017c59ceac166e4e65ee`; both are UTF-8, BOM-free, LF-only, and end with LF. The repair adds production-Host SVG rejection/atomicity evidence for representative N00/N11/N12 cases, exact Stroke operation-set/count oracles, and a focused Curve/Stroke UI contract covering cap×join, miter 1/4/1000, Binding/Expression preservation and source-value updates, stale/gesture rejection, UI Undo/Redo, native/recovery roundtrip, and deleted/reopened identity. Focused final Release is 8/8. Final full Release CTest is 41/41 in 34.63 seconds; `assets_desktop_contract` passed in 0.99 seconds with no timeout. Visible Windows T7 receipt is `build/stroke-cp2-r4-visible-evidence-20260924-221850/stroke-visible-t7.json` with platform `windows`, GUI visibility, cap/join/miter v2, Undo/Redo, native/recovery reopen, PNG pixel checks, SVG style export and selection-overlay exclusion all PASS. An earlier full run exposed one test-owned scratch-directory initialization RED; the isolated rerun and final full run passed after the test-only initialization fix. Historical S0 focused baseline 5/5 remains separate.

**Next task:** Record the single coherent repair commit, push `codex/practical-alpha`, verify exact remote HEAD/readback once, then stop without advancing to the next feature.

**Approach:** Keep the existing window/core/SVG ownership and add only bounded evidence. Validate canonical fixture bytes before parsing; use production `Host::import_svg` for representative negative rejection and document/revision/History/native/recovery atomicity; inspect actual Stroke operation sets/counts. Reuse the shared Inspector commands for cap×join, miter boundaries, Binding/Expression updates, stale and active-gesture rejection, Undo/Redo, and fresh Session identity. Exercise a task-owned visible Windows Window for T7 GUI/persistence/PNG/SVG evidence. Do not reproduce core stroke semantics in adapters or weaken existing tests.

**Done for next:** MUSE2-R1 through MUSE2-R5 are `FIXED_VERIFIED` by the focused/full Release evidence and the visible Windows T7 receipt. Commit and remote readback remain the only closeout actions. Do not advance to the next feature.

**State:** `D:\Documents\Nect`, `codex/practical-alpha`, reviewed candidate `ac763d6c4fcea82e468aeccc61428be97980e845` → this bounded repair checkpoint commit; no production source file changed. r4 build artifacts, exact fixture scratch and visible evidence are ignored; no owned GUI/test process remains. Exact local HEAD and remote pointer are recorded in the closeout report after readback.

**Authority:** User-started NECT-STROKE-CP2 r4 / READY and the exact MUSE2-R1..R5 repair request permit bounded test/evidence changes, a coherent working-branch commit and push. The exact inline r1/r2 bytes are verified, so the fixture stop condition is cleared. No main merge, release, distribution, policy/account changes, paid dependencies, shared-history rewrite or next feature.

**Handoff:** `CONTINUE_CURRENT_TASK`. Owner documents: AGENTS.md, START_HERE.md, docs/model-v0.md, docs/svg-import.md, docs/quality.md and docs/first-usable.md. Stop after this packet; do not start the next feature.
