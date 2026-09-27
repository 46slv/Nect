# R04-FOLDER-EMPTY-01 — Confirmed Folder entry

Status: Sol-frozen implementation packet, 2026-09-27. Baseline `codex/practical-alpha@f66a15eab75f2fe4db671cd2a31aab7371efd897`, native 0.20.

Authority: DEC-71 Mission ownership; [REQ-30](https://app.notion.com/p/3e0fd279a6f3818db9c0e0a1fc82a901), [REQ-52](https://app.notion.com/p/3e0fd279a6f38151b544dda1a39adf25), and [P04-A](https://app.notion.com/p/3e3fd279a6f381ee8ab0ea5ea614c006). Folder is the existing `Kind::group` in `ARCHITECTURE.md`; Structure remains distinct from Transform Parent, Collection and definition/instance.

## Scope and contract

Add one shared Session command to create an empty, neutral Folder in a specified Composition and structural parent. It appends after that parent's existing children or Composition roots. The new Folder has a stable caller-supplied ID, name, empty children, identity transform, default visible/normal/pass-through compositing and no effects. It creates no second Folder model or native version. Expose the command through the JSON-lines API and a desktop Edit action, using the current Canvas drill scope. Select the new Folder in the tree/canvas so the existing Rename, Reorder and bounded Put Inside actions can organize it. Use Folder in the new action's user-facing label.

Positive oracle: given roots A,B, `CreateFolder(comp, root, F, "Folder")` produces A,B,F without changing A/B bytes, world transforms, render or property references. `PutInside(comp, root, F, [A,B])` then moves the immediately preceding A/B into F while preserving their IDs, refs, world transforms and paint order. Each command has one exact Undo/Redo boundary, and the result survives native 0.20 encode/decode or cold reopen. UI action uses that same command and expands the ancestor of the newly selected Folder so its row is visible.

Negative oracle: duplicate ID, missing/foreign Composition or parent, and stale revision reject atomically with no authored delta. This packet does not add general cross-parent reparenting, compositing conversion, Collection membership, definition/instance, or Candidate REQ-54/56–59 behavior.

## Acceptance and residual

Focused core/API, desktop action, native roundtrip and existing affected structural tests pass; Sol reviews the exact candidate. This closes only the empty Folder creation entry of Confirmed REQ-30. Full REQ-30 still needs convenient movement into/out of Folder and live tree collapse/expand evidence. REQ-52's neutral contiguous grouping exists; existing PutInside checks world transforms but does not inspect destination visibility/compositing, so an unsafe-appearance refusal or explicit conversion path is a separate R04 packet. General reparent appearance handling remains open. R03 hands-on GUI is LOCAL_WAIT, and offscreen Qt tests do not close it.

## Local result — 2026-09-27

The reviewed candidate adds `CreateFolder` to the shared Session command variant, JSON-lines parser and History label, with a Desktop Edit action scoped to the current Composition/parent. It uses the existing Group native shape and version 0.20. Core/API tests cover exact neutral state, stable authored objects/references, paint order, immediate Put Inside continuation, separate Undo/Redo, native encode/decode, duplicate and foreign identity, and stale revision rejection. The Desktop interaction test covers action reachability, current drill scope, selected/visible tree row, pixel-identical render, and Undo/Redo. Release build targets `compositing_tests` and `window_tests` passed. Focused `compositing_contract` and `window_interaction` passed 2/2; after Sol changed the selection reveal policy, `window_interaction` passed again 1/1. `git diff --check` passed. This is offscreen Qt/API/native evidence, not hands-on GUI acceptance.
