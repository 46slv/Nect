# R12 existing History safety-oracle slice

Status: BUDGET/GESTURE SUBSET QUALIFIED; FULL TEXT CONTRACT STILL BLOCKED. Base `eb58f7ec780a960176c4e37d96d18780600db3bb`.
Owner: dot, sole cloud writer. Existing History/atomicity quality acceptance only.

The default History main executes long_history and authored_roundtrip, then reaches
unsupported Text initialization on Linux. Later original safety functions were not
executed by that failed full run. The new strict `--budget-gesture-only` invocation
runs these existing functions without changing their bodies or any assertions:

- point_edit_enabled_history: driver/Ref byte accounting, atomic over-budget refusal,
  Undo/Redo of linked and frozen literal state
- group_path_follow_history_accounts_for_relation_items: retained relation-map and
  child-ID accounting, exact redo and HISTORY_LIMIT rejection
- limits_and_gestures: pruning, earliest retained boundary, redo preservation after
  failed branches, failed gesture commit/preview consistency, cancel/invalid preview,
  many previews committing once, no-op/reset behavior and failed-batch atomicity

Register `history_budget_gesture_contract` separately. The no-argument full
`history_contract` keeps the exact original call order and still includes Text.
Unknown modes fail; subset output explicitly excludes Text/full acceptance. This
adds execution of previously obscured existing oracles, not a smaller substitute
for the full obligation. No production code/schema/native0.77 change is planned.

## Final cloud receipt — 2026-10-01

- Baseline attempted subset argument ran the old main and failed at DirectWrite,
  after its storage measurement; the later three functions were not reached.
- New named subset PASS34 checks: Point Edit7, Group-follow relation accounting5,
  limits/gestures22. All source before main is byte-identical to the parent commit.
- Default full test keeps the original six-function sequence and fails at the same
  Text platform boundary. Unknown, empty, repeated and extra arguments all return1
  with usage before executing tests. No production code or expected assertion changed.
- Final all-target desktop build PASS; full serial71 CTest44pass/1skip/26fail,
  unchanged failed-name set from70-test SVG checkpoint. No aggregate/Text/Windows
  acceptance is inferred from the independent subset pass.
- Independent read-only review found no blocker and verified exact original helper
  bodies, default order, whitelist arguments, CTest naming and static34-check count.
- Evidence: ignored `build/d0-evidence/history-budget-{red,subset,invalid-mode,all-build,all-tests}.log`.
- Next eligible existing-oracle slice: Duplication's visibility and Point Edit driver/
  expression remapping functions lie after an initial WIC fixture failure. Qualify
  those unchanged independent functions separately; retain the default full contract.
