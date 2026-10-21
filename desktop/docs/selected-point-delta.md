# Selected owned-point delta

Base: PR41 `b7fff32a995b294be211628cb42f0e1794947653`, branch
`codex/contour-point-navigation`. The point panel has individual positions,
view-only unit/contour filters and paging; common owned-point delta inputs are
absent. Generated Path Remap already has its independent checkbox batch control.

Source: [Nect Home](https://app.notion.com/p/3dffd279a6f381cca8c7c4dec111b131)
adopted point-editing goal and research08 sections 5/7 in
[research](https://app.notion.com/p/3fcfd279a6f381e78204fa7e1053818b).
Multiple selected points need common ΔX/ΔY, rather than collapsing their distinct
absolute positions. The existing node output coordinate space is the default;
relative handles stay unchanged. A locked/driven member rejects the entire group.
One numeric confirmation is one Undo; Escape discards the draft.

Add two delta inputs and Move points / Cancel inside the existing point panel
for selected owned Curve/Multi Path points. Label the coordinate space
“Node-local · document units”. Operate on all selected stable IDs, including
points outside the current browsing filter. Keep whole-unit actions and generated
Remap controls separate. No model/schema, bake/rebind, world-space conversion,
new panel, or dither changes.

Drafts capture layer, node, selected point IDs and Session revision on first input.
Both axes commit together through the existing atomic `translate-points` command;
blur only stages text. Enter or Move points commits, Escape or Cancel discards.
Invalid numbers and stale commands leave document/history unchanged. A changed
selection discards its old draft. Zero delta preserves Undo/Redo. Both panel
copies share draft values; IME Enter/Escape do not commit/cancel mid-composition.

Verification plan: compound contours including a hole and an open contour,
nonzero handles, unselected geometry, one-step Undo/Redo, cancellation, zero delta,
invalid/stale/locked/driven/missing targets; DOM interaction and task-owned native
WebView2 interaction. JSON disk reload is distinct from native OS Save/Open or a
cold process reopen. Preserve sealed PR40/41 candidates.
