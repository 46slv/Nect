# Canvas artwork bounds marquee

Base: PR46 `codex/named-node-groups`, verified remote HEAD
`1ea5ecc4724ff041a0b452f7fc5f165d4bc62398` at
`https://github.com/46slv/Nect.git`. The current parent brief selects this new
scope. Current Graph already has marquee/overlap/Shift behavior; Canvas Select
has whole-artwork click/Shift-toggle and drag movement, but no blank-drag marquee.
Graph marquee stays unchanged.

V1 selects whole displayed artwork layers by intersection with their existing
`pathBounds` (the same bounds used by artwork selection overlays), not precise
shape/segment intersection. The UI identifies bounds overlap. Capture only stable
IDs actually rendered on the current Canvas and passing existing `layerShown`;
deduplicate nested render fragments of a referenced/compound output to its outer
layer identity. Hidden, solo-excluded and other project compositions are absent.
Visible locked, driven, generated, image and reference outputs remain selectable
under the current whole-artwork contract; existing move/edit guards still decide
whether a subsequent edit is supported. No bake or model rewrite.

Only Select-tool primary drag from blank Canvas starts a marquee, after existing
navigation, text/character, guide and dedicated handle/anchor hit routes. Capture
the starting selection, Session/revision and targets once. Four screen pixels
distinguish drag from click; below threshold preserve existing blank-click clear
(including Shift). A normal drag replaces selection; Shift adds to the starting
whole-artwork set, never toggles based on intermediate hits. Clamp the moving end
to the Canvas viewport and use the existing SVG screen-to-document transform.
Inclusive AABB overlap includes touching/zero-width line bounds. Empty result
clears selection; active artwork follows retained active ID or the last hit.

Preview is detached: update only one rectangle/count and candidate-bounds overlay,
with no full render, graph scan, evaluation or bounds measurement per pointermove.
Commit once on release through existing artwork selection ownership; Canvas,
Layers, Graph and Inspector then synchronize. Escape/pointercancel discards the
preview and restores the starting selection. Stale document/revision/Canvas
targets discard without overwriting newer selection. Selection never changes
geometry, dirty state, schema or Undo. Existing target dragging/pan/tools and
anchor/segment editing stay separate.

Focused tests cover transformed coordinates, reverse drag/overlap, replacement,
Shift union from the start set, empty results, threshold clicks, cancellation,
locked/hidden/generated/reference identity, stale targets, selection synchronization
and target-drag/pan/point-edit coexistence. Native checks use actual Tauri/WebView2
with synthetic CDP input; physical input and OS dialogs are separate. Preserve
PR40 through PR46 sealed candidates before the next native build. No panel redesign,
anchor marquee, cutting/deletion or new multi-layer transform capability.
