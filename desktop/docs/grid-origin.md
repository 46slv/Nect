# Nine-point grid origin

Bounded brief: PR60 base `e169a84a3ddf8823086825050ed8f37339f086aa` has only
top-left/center in guides.js and a Center grid button. Extend only the grid's
phase, through its shared display/snap resolver and existing Session setting.
This is the parent's selected production improvement; a Notion Candidate is
not being treated as approval.

Grid panel **Origin** chooses top-left, top-center, top-right, middle-left,
center, middle-right, bottom-left, bottom-center or bottom-right. The stored
anchor resolves to canvas width/height multiplied by 0, 0.5 or 1. Resizing the
canvas keeps that relative anchor; no absolute origin is stored. Spacing,
opacity and snap priority/bypass are unchanged.

Legacy missing anchors and top-left stay top-left; new blank documents remain
centered. Unknown resolver input still falls back to top-left, while invalid
authored values reject atomically. Existing top-left/center documents retain
their incoming versions. The seven added anchors require schemaVersion 48;
center still requires 27. Selecting an anchor uses one existing Undo step;
choosing the same setting remains a no-op.

Artwork, guide positions, ruler/world coordinates and SVG output do not change.
There is no new origin for any other system, panel redesign or geometry edit.
Acceptance covers all nine anchors, fractional/different canvas sizes and
resize, legacy/default/schema rejection, JSON/Undo/Redo, display/snap agreement,
unchanged artwork/SVG, and representative corner/edge/center native renderer
interactions with zoom/pan. Native input is CDP synthetic; physical/OS input,
dialogs and normal OS cold reopen remain unverified.
