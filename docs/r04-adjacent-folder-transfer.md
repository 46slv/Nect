# R04-ADJACENT-FOLDER-TRANSFER-01 — move a suffix to the next Folder

Status: Sol-frozen implementation packet, 2026-09-27. Baseline clean `codex/practical-alpha@e3629c5eb717f23e38788e7add0ea2f07448eb4e`, local/tracking/fresh remote equal. Single-writer TAKEOVER_ACK: `build/manual-recipes/r04-folder-takeover-ack-20260927.json`; predecessor RELEASE received.

Authority: DEC-71 Mission; Confirmed [REQ-30](https://app.notion.com/p/3e0fd279a6f3818db9c0e0a1fc82a901) Folder movement and content/reference preservation; Confirmed [REQ-52](https://app.notion.com/p/3e0fd279a6f38151b544dda1a39adf25) appearance/order and explicit refusal; [P04-A](https://app.notion.com/p/3e3fd279a6f381ee8ab0ea5ea614c006) operation audit. This is one entry in R04, not either Requirement's full acceptance.

## Contract

Expose an Edit and selection-menu action, “Move selected to next Folder”. Selection must be whole objects forming an ordered suffix of one source Folder. That Folder and the destination Group Folder must be adjacent siblings in the active Composition, with destination immediately after source. Run the existing `MoveOut(..., "after")` then `PutInside(...)` in one `Session::apply` call and one Undo boundary. Keep moved IDs, properties, references, Collection membership, world transforms, drawn pixels and paint order. Preserve both Folders and use the existing native writer. Strengthen `PutInside` to reject a dependent value or surviving world-transform change after local affine correction. The current Canvas and Structure selection follows the moved objects to the destination Folder.

Reject a middle/prefix/noncontiguous/mixed/point selection or missing next Folder with a scoped selection error, without any authored delta. Existing neutral-source, destination-appearance, driven-property, singular-transform, dependency and revision checks still apply. If either command fails, the Session rejects the entire edit without changing Document, revision, History or recovery. JSON-lines clients can invoke the same two commands in one `apply` request; no second structural command model is introduced.

## Oracle and exit

Given roots `X, F1, F2, Y`, F1 children `A,B,C`, F2 children `D`, moving selected suffix `B,C` produces roots `X,F1,F2,Y`, F1 child `A`, F2 children `B,C,D`. Render before/after is pixel-identical and every world transform and evaluated property is unchanged. One Undo/Redo restores exact states; native encode/decode and a separate process reopen retain them. Negative fixtures include middle child, nonadjacent target and unsafe destination with atomic failure. Focused offscreen Desktop and core/API tests must pass, followed by source review and non-force remote SHA readback.

Residual: arbitrary cross-Folder reparent, nonadjacent moves, placement into a preceding Folder, tree drag, live hands-on collapse/expand, and broader R04/REQ-30/52 acceptance remain open. Candidate REQ-46–59 are not adopted by this packet.

## Local result — 2026-09-27

Sol reviewed the exact diff. The Edit and selection-menu actions call one Session edit composed from `MoveOut` and `PutInside`; the shared `PutInside` guard now rejects changes to surviving world transforms and dependent property values. Core/API tests cover an existing destination child, order, world coordinates, Collection membership, stable external reference, native encode/decode, atomic unsafe-destination and driven-source refusal, and one Undo/Redo. Offscreen Desktop tests cover Edit/context reachability, selection and Structure row, pixel-identical render, point/non-suffix refusal and one Undo/Redo. Release focused CTest passed 2/2 and full serial Release CTest passed 41/41; the final fixture addition to the core test passed its affected test 1/1. Distinct-process JSON-lines/native 0.23 cold reopen matched exactly, SHA-256 `e1e9b48c506aa40241ed0f5fcd997ac4b49b8329827fb4c1497cea81ec20bf67` at ignored `build/manual-recipes/r04-transfer-cold.nect`. No live hands-on GUI acceptance is claimed.
