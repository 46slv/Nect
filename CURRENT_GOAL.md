# Area-selection Mission

## Mission Brief

**Mission identity:** `Nect/area-selection`; same primary executor / Task / working branch.

**Final Goal:** Select useful subsets of artwork with one rectangle gesture, reducing repeated Shift-clicks in ordinary multi-object and point editing.

**Completion conditions:** Empty-Canvas drag selects contained visible whole objects at the current Group scope, or point anchors in current point-edit targets. Shift extends selection. Click and Escape remain predictable. No authored revisions/history changes. Actual Windows GUI, focused interaction contracts and proportional viewport evidence establish behavior.

**Constraints / Authority:** Current continuous-development instruction permits reversible UI choices and working-branch commits/pushes. Accepted direct-editing/group selection direction in `docs/product-direction.md`. No main push/merge, releases, new dependencies, external changes or new Tasks. Selection is view state, not another Session authoring authority.

**Coarse checkpoint map:** (1) Rectangle gesture, clear contained-bounds contract and visual feedback. (2) Verify real production selection/Undo interactions and repair any exposed issues; reassess next Mission here.

## ACTIVE CHECKPOINT

**Goal:** Add rectangle selection on empty Canvas for visible scoped objects and current point-edit targets.

**Current phase:** Direct-edit-flow accepted; synchronize keyboard checkpoint then implement rectangle selection.

**Proven:** Selection/framing synchronized `f7e3fef`. Keyboard focused tests2/2 in4.21s and actual GUI retained Rectangle/Point Edit/Undo saved native recorded in `docs/first-usable.md`. Existing Canvas has scope-aware selection and multi-selection commands but empty-space mouse press only clears selection; no rectangle gesture.

**Next task:** Inspect current mouse/drag cancellation and overlay paths; implement screen-space rectangle with full containment, Shift-add and release-only selection commit.

**Approach:** Reuse selection_target and evaluated geometry caches. Group ancestors are single targets, hidden/mask-only geometry excluded. Point mode preserves frozen target objects. Selection updates only at release to avoid rebuilding Inspector during drag. Escape restores prior selection; no semantic gesture or history entry.

**Done for next:** Direction-independent rectangle, scope/points/hidden targets, modifier behavior, cancellation and unchanged Session state proven in focused tests and actual GUI. Check relevant viewport behavior; commit/push and verify remote HEAD.

**State:** `D:\Documents\Nect`; `codex/practical-alpha`; keyboard changes ready to commit; prior remote `f7e3fef7522bf116aedfdcca9b6fda6ed819f50b`. Owned GUI closed. Full access / never approvals.

**Authority:** Current explicit continuous-development user instruction. Continue after checkpoints and Mission boundaries.

**Handoff:** CONTINUE_CURRENT_TASK; no successor Task.
