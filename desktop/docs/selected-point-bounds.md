# Selected-point bounds

Base: verified PR42 head `b4a6397db111fb10b9e83c6511cb3e6c8e62ca64`,
`codex/selected-point-delta`. Current panels have selection count and common delta
inputs, but no selected-anchor bounds. PR42's EXE and receipt are sealed in
`C:/Users/shiro/Documents/Codex/2026-10-21/task-13/selected-point-delta-artifact-cc208bd`;
SHA256 `8f52b65a5e0f413b35a66155d6aa8da403b8f98eb92e2d4160af587403e77f85`
was verified before building another candidate.

Source: [Nect Home](https://app.notion.com/p/3dffd279a6f381cca8c7c4dec111b131)
adopted point-editing goal, research08 section5 in
[research](https://app.notion.com/p/3fcfd279a6f381e78204fa7e1053818b): multiple
points expose count/bounds/common delta, defaulting to this node's stage.

Add a compact read-only min/max X/Y, width and height summary beside the existing
delta controls. Label count and Node-local document units. Stable Canvas point IDs
own membership, including selected points outside browsing filters. Reuse
`unionBounds` with zero-size anchor boxes, not contour/Bézier/stroke bounds.
No geometry/schema/Session mutation, transform, bake/rebind or panel redesign.

Only fully editable Curve/Multi Path selections produce numbers. Empty, missing,
duplicate, locked, driven, bypassed, generated/non-owned, invalid or overflowed
selections show a compact reason with no partial numbers. Handle controls do not
affect anchor bounds. Display uses six significant digits with exact values in
output titles; zero-size selections remain valid. Recompute from current state
on existing selection/point-panel/command renders; no saved bounds cache.

Focused acceptance: owned single/multiple/cross-contour selection, off-list
membership, handles/stroke/downstream exclusion, point-value edits, delta and
Undo/Redo, empty/rejected/stale states and document/history neutrality. Use the
existing task-owned native WebView2 route; distinguish synthetic renderer/disk
checks from unavailable physical input or OS file dialogs. Preserve PR40–42
sealed candidates.
