# R04-MOVE-OUT-01 — bounded Folder extraction

Status: Sol-frozen implementation packet, 2026-09-27. Baseline `codex/practical-alpha@0b2459a8d27cb2edeac06b6e72032d4b8198bc5f`, native 0.20.

Authority: DEC-71 Mission ownership; Confirmed [REQ-30](https://app.notion.com/p/3e0fd279a6f3818db9c0e0a1fc82a901) movement without loss of content/property/references and [REQ-52](https://app.notion.com/p/3e0fd279a6f38151b544dda1a39adf25) appearance/order preservation. [P04-A](https://app.notion.com/p/3e3fd279a6f381ee8ab0ea5ea614c006) calls for an operation-level audit. Existing `Ungroup` proves the neutral/static affine/value-preservation boundary, but deletes the Group. This packet leaves the Folder in place.

## Contract

Add one shared `MoveOut` Session command with Composition, structural parent of the Group, Group ID, ordered contiguous member IDs, and placement `before` or `after`. The Group must be a visible, neutral, static container with no explicit Transform Parent, opacity/blend/isolation/mask/effects or driven Group transform. `before` requires the members to be a prefix of the Group's children; `after` requires a suffix. Place them next to the Group in its parent's sibling list. Empty Group remains valid. Preserve every moved ID, authored geometry and non-transform properties, Collection membership, explicit Transform Parents and references. Adjust local affine values only as needed to preserve world transforms. Verify evaluated property values and all surviving world transforms; reject dependencies that cannot be preserved atomically. No cross-Composition movement or lossy conversion.

Expose this command in JSON-lines and Desktop Edit/selection menu as “Move selected out of Folder”. Desktop may infer `before` for a selected prefix and `after` for a suffix; for the whole child list choose `after`. A middle block, mixed parents, point selection or other unsupported selection refuses with a scoped error. Do not silently reorder noncontiguous selections. Use the same Session command and one Undo boundary on every surface.

## Oracle

Positive: parent roots `X,F,Y`, F children `A,B,C`, neutral F with nonidentity invertible affine. Moving `[A]` before yields `X,A,F(B,C),Y`; moving `[C]` after yields `X,F(A,B),C,Y`. Drawable paint order, every world transform, authored IDs/geometry, properties/references and Collection membership stay equivalent. A whole-child move leaves an empty F. Undo/Redo exactly restores both states, and native 0.20 encode/decode retains the result. JSON-lines and desktop action reach the same command.

Negative: hidden/non-neutral/dynamic F, middle or noncontiguous child block, wrong parent/Composition, driven moved affine, dependent property change, stale revision and malformed placement reject atomically with stable error codes and no Document/revision/history delta. The exact error names may reuse the existing `UNGROUP_*`/`TRANSFORM_PRESERVATION` boundaries where semantically identical, with a new `MOVE_OUT_*` code for selection or placement. No general reparent, arbitrary cross-Folder move, Collection/definition/instance behavior or Candidate REQ-54/56–59 scope.

## Acceptance and residual

Focused core/API/native and Desktop interaction tests pass after Sol review. This is a bounded part of REQ-30 and REQ-52, not either full Requirement: arbitrary Folder move, UI drag workflow, live GUI collapse/expand and general reparent remain open. R03 hands-on GUI remains LOCAL_WAIT.

## Local result — 2026-09-27

The reviewed `MoveOut` command leaves the Group and its references in place, moves only an ordered child prefix/suffix beside it, and verifies every surviving world transform and unchanged evaluated property. JSON-lines and the Edit/selection menus call that Session command. Focused tests cover prefix, suffix and whole-child moves, nested Canvas drill scope and Structure tree selection, compositing/selection/dynamic/dependency refusals, stable IDs/Collection membership, one Undo/Redo, and native 0.20 encode/decode. Release `compositing_tests` and `window_tests` built; `compositing_contract` and `window_interaction` passed 2/2. `git diff --check` passed. These are API/offscreen Qt/native checks, not live hands-on GUI acceptance.
