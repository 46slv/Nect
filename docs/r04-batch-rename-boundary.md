# R04-BATCH-RENAME-01 — selected sibling rename

Status: Sol-frozen bounded implementation packet, 2026-09-27. Baseline `codex/practical-alpha@6aeb5e4d04322cd307171b3877cb3213e1790946`, native 0.20.

Authority: DEC-71 Mission ownership; Confirmed [REQ-36](https://app.notion.com/p/3e0fd279a6f3815bbbaed3bddf04d3f5) requires reusable batch organization reachable from automation as well as UI. [P04-D](https://app.notion.com/p/3e3fd279a6f381ee8ab0ea5ea614c006) calls for old/new name preview, stable IDs, atomic apply, three-target/duplicate-name/failure/Undo/native/API oracles. Existing shared `Rename` commands already support atomic `Session::apply` batches and JSON-lines `commands` arrays. This packet exposes that capability to multi-selection in Desktop; it does not close all REQ-36 sort/group/Folder/Collection obligations.

## Contract

Add an Edit and selection-menu action `Batch rename selected…` for two or more whole sibling objects in the active Composition. Resolve selected IDs in structural sibling order, including a single Folder's children, using the existing selection helper. Open a dialog showing stable ID, current name, and an editable proposed name for each row. Propose deterministic numbered names using a user-editable base name and one-based sequence in sibling order; allow duplicate proposed names when the user edits rows. Recompute only unedited proposed rows when the base changes. No Document mutation occurs while reviewing or cancelling the dialog. Apply an ordered vector of existing `Rename` commands through one `Session::apply` call at the captured session/revision, so one Undo restores all names and a stale dialog refuses without partial change. Empty proposed names are invalid in this tool and show a local error while keeping the dialog open. Keep selection stable after apply. No new core command or native version.

## Oracle

Three selected siblings A/B/C preview old names and proposed names in A/B/C order, even when selection clicks occurred out of order. Editing B/C to the same name keeps that row order; one apply changes exactly those three `Object::name` values while IDs, property references, Structure, paint order, rendered state and native 0.20 source remain otherwise equivalent. One Undo/Redo restores/reapplies the batch; save/reopen preserves names and stable refs. The equivalent JSON-lines API batch of three `rename` commands reaches the same Session result. A missing middle target or stale revision fails atomically with unchanged Document/revision/history; cancelled dialog and empty-name attempt do not mutate. Cross-parent, point, or fewer-than-two selection is not offered/accepted.

## Residual

Sorting by displayed name versus paint order, batch grouping/Folder moves, Collection scope, arbitrary non-sibling selection, preset/action automation and Candidate REQ-37/146 remain separate. Offscreen Qt/API/native checks do not establish live hands-on GUI acceptance or full REQ-36 completion.

## Local result — 2026-09-27

The reviewed Desktop Edit and selection-menu actions present stable IDs, old names and editable proposed names in structural sibling order. The proposal list scrolls for larger selections. Only Apply sends ordered existing `Rename` commands through one `Session::apply` and one Undo boundary. The action rejects invalid selection, empty proposed names, stale revision and replaced Session without partial target changes. JSON-lines accepts the same atomic command array. Release `window_tests` and `compositing_tests` built; `window_interaction` and `compositing_contract` passed 2/2 after Sol review. Tests cover exact Document delta, reference/value/render and Collection preservation, native 0.20 file save plus independent Host cold reopen, Undo/Redo and negative paths. `git diff --check` passed. Live hands-on GUI acceptance remains unverified.
