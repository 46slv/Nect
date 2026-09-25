# Nect — autonomous Mission ownership / R03 daily editing

## Mission Brief

**Final Goal:** Advance Nect's accepted Completion Route through eligible design, implementation, review, repair and validation steps under Codex Sol Mission ownership. Preserve all Confirmed Requirements and do not claim an L1/L2/L3 line complete without its own acceptance.

**Completion conditions:** Each material candidate has an exact packet, independent positive/negative acceptance, Sol review and necessary repair; code, native/API/GUI/runtime evidence and Requirement residuals are reconciled; coherent checkpoints are synchronized and read back. Continue to the next eligible Route step rather than treating a packet, commit, review or Task boundary as Mission completion. Actual human hands-on or preference acceptance remains explicit.

**Constraints / authority:** Current user instruction and [DEC-71 Accepted](https://app.notion.com/p/3e6fd279a6f381219079f6b9a024fece) establish Codex Sol as Mission Owner. Sol may resolve technical ambiguity within accepted intent, approve/update bounded packets, use fresh disposable Luna Max Workers, review/repair, and perform ordinary reversible Git lifecycle including required-check non-force merge. Human boundaries: actual hands-on/subjective acceptance, material preference-dependent product choice, major Goal replacement/expansion, platform confirmation, credentials/account/billing, destructive/irreversible action and release/publication/public external action. Preserve user projects, worktrees, settings, Confirmed Requirements and non-goals. Notion owns product decisions; Git owns implementation truth.

**Coarse checkpoint map:** P01 qualification/remote closure (done) → P02 design/A native/B overlay/C1 Smart Snap/C2 align and distribute/D daily UI first slice (implemented and runtime qualified with residuals) → R11-Q interop/host feasibility (qualified with explicit R11-I design gates) → R02 first typed Color discovery slice (synchronized; broader typed dependency/save DESIGN_GATE remains) → R03 daily editing and layout using the existing Scalar/command contract → later eligible Route steps by dependency and value. This order may change with fresh evidence. Keep only one active checkpoint detailed.

## ACTIVE CHECKPOINT

**Goal:** Resolve the reproducible Offset UI Stack Jump crash exposed by the R03 validation suite, preserving authored state and the completed baseline Snap behavior.

**Current phase:** [R03-BASELINE-01](docs/r03-baseline-entry.md) is verified for its scoped Text baseline behavior: focused 3/3, actual GUI/API/native cold reopen and paired visible performance PASS. The full suite remains 40/41 because `offset_ui_interaction` crashes both before and after this slice. That independent existing UI defect is the next bounded repair target; rotated/vertical baselines remain residuals.

**Proven:** Previous repeated-gap closure remains at `6e0ebc18bda37890cbdd1785c106ebedf9f2bdd3`; see [R03-ENTRY-01](docs/r03-entry.md). The baseline slice's Release focused Text/Canvas 3/3 passed. Owned GUI feedback named target Text line 2; API revision 4 and native 0.14 independent cold reopen retained `tx=0,ty=96` for Document `2ecc8af7-de9a-42dd-b75a-2567f30c3b06`. The paired unchanged-HEAD/current-candidate visible benchmark representative object-translation p95 was 20.58/20.16 ms, both below 33.3 ms. Full CTest was 40/41; the same Offset UI SegFault reproduced on the unchanged HEAD, so it predates this slice. Full R03/REQ-33, R11-I and R02 typed dependency/save acceptance remain open.

**Next task:** Freeze a small `offset_ui_interaction` repair packet for the preexisting Stack Jump crash, attribute the exact Qt event/lifetime failure, fix it without weakening the test, and run focused Offset UI plus proportionate desktop regression. Preserve the completed baseline packet, then continue eligible R03 Route residuals.

**Approach:** The crash occurs after a Stack Jump QAction triggers and before the subsequent Qt event drain, with `Qt6Widgets.dll` 0xc0000005. Use an owned UI test or debugger to identify whether a stale menu/widget, deferred reveal or focus/scroll event causes it. Keep the repair separate from Text layout and preserve the Offset source, Undo and Session contract.

**Done for next:** `offset_ui_interaction` passes without reducing its assertions, the full relevant suite is rechecked, and actual UI behavior remains coherent. Commit/sync/read back the exact repair and reconcile Notion. Do not claim full REQ-33/R03 from a slice.

**State:** Repository D:\Documents\Nect, branch codex/practical-alpha. The previous Owner released the single-writer role after `build/r03-baseline-takeover-ack.json` readback. R03-BASELINE-01 implementation/evidence commit `36064f54f5a5e3058f73c3d1ccb43f88bb5e8327` was non-force pushed and matched local/tracking/fresh remote with a clean tree; Notion Home and Worker packet were read back with that exact semantic SHA. Fresh-read branch HEAD before the next mutation. Owned GUI processes are closed; baseline comparison sources/build and fixture/recovery artifacts remain ignored under build/.

**Authority:** Current user standing Mission instruction, 2026-09-26 token-efficiency supplement, DEC-71, scoped live Requirements/Route and repo AGENTS.md. Reversible work and normal non-force branch sync are authorized; Human/destructive/public boundaries in the Mission Brief persist.

**Handoff:** CONTINUE_CURRENT_TASK. The Offset UI failure is a concrete adjacent repair with the same owner and repo state.
