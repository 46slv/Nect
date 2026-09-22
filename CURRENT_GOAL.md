# Practical-alpha Mission

## Mission Brief

**Mission identity:** `Nect/practical-alpha` — durable owner: this file on `codex/practical-alpha`. Every successor Task belongs to this same Mission.

**Final Goal:** Make Nect usable for everyday 2D graphics production as a practical alpha.

**Completion conditions:** A representative editable composition can be created and revised through the Windows GUI, protected/saved/reopened, and exported without loss of native intent. Geometry, Text, images, color and compositing work together; GUI/API/MCP share Session semantics and unsupported requests fail explicitly. Current contracts pass and representative warm interaction meets the recorded 30 fps floor. Close concrete daily-workflow blockers, not the whole backlog.

**Constraints / authority:** The 2026-09-22 user instruction resumes the Mission and supersedes the image-slice stop. Astra is primary executor. Preserve existing work; reversible local UI decisions and checkpoint commits/pushes to `codex/practical-alpha` are authorized. No main push/merge, release, new dependencies, proprietary assets or account-policy changes. Notion owns product intent; `docs/product-direction.md` is the accepted scoped extract. Native authored state remains authoritative.

**Coarse checkpoint map:**
1. Live capability/gap reassessment and image-workflow baseline — completed; evidence in `docs/first-usable.md#practical-alpha-live-reassessment--2026-09-22`.
2. Gentle Snap for everyday object placement — active.
3. Close the next demonstrated revision/output handoff gap. SVG exists; object duplication, PNG output and vector import do not. Choose one bounded slice from use, with explicit reference/unsupported semantics.
4. Integrated authoring, recovery, output and performance acceptance; reconcile remaining limits against practical-alpha completion.

Far checkpoints remain provisional. Inherited Artboard content, full AI/PSD, full node graphs, all blends, pixel painting and all Candidates are not automatic gates. The current capability/gap table lives in `docs/first-usable.md`, not a copied backlog here.

## ACTIVE CHECKPOINT

**Goal:** Make ordinary object placement easier with a visible Snap ON/OFF control and gentle screen-space attraction, while preserving exact numeric/API edits and one-gesture Undo.

**Current phase:** Ready for fresh-context implementation. The preceding reassessment and diagnostic hardening are complete. This checkpoint is selected, not implemented; do not claim Snap exists. The completed predecessor checkpoint is `54757fa`. Under the user addendum, this Snap checkpoint must run in a new Task; the current Task only synchronizes and dispatches this handoff.

**Proven:**
- Product implementation baseline: `195e536`, `ab3fcec`; latest-main integration: `ffa9471` (merges `6324126`, AGENTS-only tree delta).
- Current Release build and authoritative primary-only suite: `build/mission-final-build.log`, `build/mission-final-ctest.log` — 35/35, 31.73 s. Test source: `tests/assets_desktop_tests.cpp`; diagnostic/timeout changes are in the commit containing this checkpoint.
- Actual Windows Inspector resize/Undo, native/recovery/reopen: `build/mission-runtime-receipt.json`; durable summary and gap table: `docs/first-usable.md#practical-alpha-live-reassessment--2026-09-22`.
- Performance boundary: asset benchmark in `docs/first-usable.md`, `build/canvas-benchmark-assets.json` (historical measured 30 fps pass, not rerun here).

**Next task:** Verify live state and implement the smallest useful object-body translation Snap slice. Start with selection geometric bounds against active Artboard edges/centers and visible unselected object bounds in the same Composition; adjust this candidate if live gesture/transform behavior makes a smaller first subset safer.

**Approach:** Read only `src/desktop/canvas.*`, the relevant toolbar/view code in `window.cpp`, `tests/canvas_tests.cpp` / transform tests, and the accepted Snap paragraph. Resolve optional snapping in view interaction before the existing `TranslateObjects` Session gesture path. Use a modest logical-pixel tolerance (initially ~6 px), deterministic target choice and subtle alignment feedback. Keep targets stable during a gesture; exclude moving objects and dependent descendants from self-targeting. Use existing shared geometric evaluation; no parallel document or persisted snap geometry. GUI/API/MCP still mutate through the same semantic commands; exact numeric commands never implicitly snap.

**Done for next:** Discoverable ON/OFF; ON attracts only near a candidate and OFF permits free placement at multiple zoom levels. Single/multi-object motion preserves relative spacing and parenting semantics. Commit is one Undo; Escape restores exactly; stale/concurrent edit rules remain intact. Focused gesture tests and real Windows interaction verify feedback/placement; measure representative interaction if the candidate scan changes the hot path. Save one coherent checkpoint, push branch and verify remote HEAD before moving on. Non-goals: grid/guide persistence, point/handle snapping, distribution, duplication, output codecs or architecture redesign.

**State:** Branch `codex/practical-alpha`, upstream `origin/codex/practical-alpha`, [remote checkpoint branch](https://github.com/46slv/Nect/tree/codex/practical-alpha). Initial clean `ab3fcece80aee06a4418d05267f899f9f942ed02`; pre-checkpoint integration HEAD `ffa9471`. Exact checkpoint SHA is the commit containing this file (`git log -1`); verify it against `git ls-remote origin refs/heads/codex/practical-alpha`. Working tree is intended clean after this checkpoint commit. Native 0.13 / raster interpretation v1; original examples unchanged; owned runtime closed normally, no pending mutation.

Known limitation: the initial suite hit one 60 s image-GUI timeout. Isolated timing narrowed the delay to Qt test file-chooser teardown; the exact cause of the timeout is unproven. Subsequent primary-only final suite passed. Flushed phase diagnostics and a 60 s CTest limit now make recurrence bounded and inspectable; no production fix is claimed. Native-dialog manual import completion was not verified; real Host/API import, Qt GUI contract and visible edit/Undo were verified. Revisit if recurrence blocks the next checkpoint. No current failing test or external sync blocker is known.

**Authority:** Current user Mission instruction, `START_HERE.md`, latest `AGENTS.md`, accepted Snap direction. Continue within this Mission; Next task is a candidate and may be replanned from live evidence. The 2026-09-22 addendum mandates **1 semantic checkpoint = 1 Task**. After satisfying Done for next: save, commit, push/verify, select the next checkpoint from live state, create its dedicated new Task tied to `Nect/practical-alpha`, then end this Task. Transfer only Mission Brief → latest active checkpoint → repo/branch/HEAD → needed owners; never fork old conversation or forward raw logs. If Task creation is unavailable, mark `WAIT_SUCCESSOR_TASK` and stop. The predecessor owns only this handoff until new-Task dispatch; it must not implement Snap. Successor uses the existing `D:\Documents\Nect` / `codex/practical-alpha` checkout after predecessor writes finish, as the sole executor.
