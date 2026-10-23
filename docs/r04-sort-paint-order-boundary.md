# R04-SORT-PAINT-ORDER-01 — selected sibling name sort

Status: Sol-frozen bounded implementation packet, 2026-09-27. Baseline `codex/practical-alpha@fc409c932282b7f30a8939420acb4982df37c3bf`, native 0.20.

Authority: DEC-71 Mission ownership; Confirmed [REQ-36](https://app.notion.com/p/3e0fd279a6f3815bbbaed3bddf04d3f5) requires reusable rename/sort/group organization and automation reachability. [P04-D](https://app.notion.com/p/3e3fd279a6f381ee8ab0ea5ea614c006) requires an old/new order preview, stable IDs, one atomic apply and a distinction between name display order and actual paint order. Existing `ReorderObjects` is the shared Session/JSON-lines command. This packet adds a narrowly labeled Desktop sort of actual paint order; it does not promise a display-only sort.

## Contract

Add Edit and selection-menu action `Sort selected by name (paint order)…` for two or more whole sibling objects in the active Composition. Resolve selected IDs in structural sibling order, including children of one Folder. Sort only the selected IDs by case-insensitive `QString::compare` on current object names, using stable sort so equal names retain their old relative order. Place sorted IDs back into their original selected sibling slots; unselected IDs retain their slots. The dialog must preview every affected sibling slot's old/new ID and name and explicitly state that Apply changes draw/paint order. Do not mutate the Document during preview or Cancel. Apply a single existing `ReorderObjects{composition,parent,full_order}` command at captured Session/revision. A stale Session/revision refuses without mutation. If the resulting full order equals the old order, show a no-change result without a Session revision or history entry. Retain the selected IDs after apply. No new core command or native version.

## Oracle

Parent order `[U, C, X, B, A, V]`, selected C/B/A where names C=`Same`, B=`alpha`, A=`Same`, gives `[U, B, X, C, A, V]` by case-insensitive ascending comparison; equal-name C/A stays in old relative order. Preview must expose the actual full paint-order change before Apply. A different click order does not change the result. Structure, IDs, object properties, references, Collection membership and evaluated values stay unchanged; only the sibling order changes and the rendered appearance may therefore change. One Undo/Redo restores/reapplies exactly, and native file save plus independent reopen retains the resulting order. Equivalent JSON-lines `reorder_objects` command applies the same full order. Cancellation, invalid selection, stale revision and replaced Session have no authored delta. No-op sort adds no revision/history.

## Residual

Display-only name ordering, descending/user-defined sort, grouping/Folder moves, Collection scope, arbitrary cross-parent selection, preset/action automation and Candidate REQ-37/146 remain separate. This slice does not close all REQ-36. Offscreen Qt/API/native checks do not establish live hands-on GUI acceptance.

## Local result — 2026-09-27

The reviewed Edit and selection-menu actions preview every sibling slot's old/new stable ID and name, explicitly identify the change as actual paint order, and stable-sort only selected IDs into their original slots. Apply calls one existing `ReorderObjects` Session command; an already-sorted selection creates no history entry. Focused Release `window_interaction` passed 1/1 after test fixture corrections; Sol added a JSON-lines `reorder_objects` positive/atomic-invalid check and Release `compositing_contract` passed 1/1. Desktop tests cover Cancel, nested Folder children, equal-name order, one Undo/Redo, native file save and independent Host cold reopen, and stale revision/Session refusal. `git diff --check` passed. This is API/offscreen Qt/native evidence; live hands-on GUI acceptance remains unverified.
