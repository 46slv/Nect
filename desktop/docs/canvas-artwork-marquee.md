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

## Verification receipt

Runtime source `8236aebdfecd4ec8dd5346d9d1ed9bcfab64b942`. Product changes are
confined to main.js, a pure AABB/union helper and scoped preview styles. Core,
schema, Graph marquee, named groups, Color and dither remain unchanged.

- New focused checks pass 10/10 (two pure helper tests plus DOM parent/seven
  cases). Coverage includes reverse/touching/line bounds, transformed SVG
  coordinates, fixed Shift union, detached preview/unchanged Graph and Inspector
  element identity, no repeated viewport bounds measurement, selection sync,
  hidden/solo/locked/generated/reference identity, blank threshold/empty result,
  Escape/pointercancel/capture loss, anchor selection preservation on cancel,
  neutral document/file/history, busy-command rejection and replaced-document/
  partial-SVG-render invalidation. Commit clears source-point selection.
- Existing nudge/busy guard and blank-click scenarios pass individually, 2/2 Node
  entries each including their parent. Their fixtures now release the pointer at
  the actual final position; selection asserts follow release rather than down.
  Existing target drag/pan/direct-anchor routes also pass the new DOM suite.
- Final actual Tauri/WebView2 passes 4/4 synthetic-CDP cases with zero page
  errors: preview/commit and locked selection; Shift union and path/generated/
  reference identities; real zoom/pan/Escape/empty result; existing target drag
  with one Undo and direct-anchor click. Selection preserves JSON, file state,
  Session revision and SVG. Both native screenshots inspected. Native PID 59908
  was closed after verifying its task-owned executable path.
- Frontend production build (142 modules), offline native release/custom-protocol
  build, native configuration, scoped formatting and diff checks pass. Existing
  bundle-size warning remains. No full DOM render or graph measurement occurs on
  marquee pointermove; geometry/bounds are captured once from current Canvas
  targets. This is not a broad frame-time benchmark.

Extended old-suite probes have known failures. Exact PR46 main.js was checked
with the same core/harness: the toolbar artwork-creation scenario, seven old
multi-artwork snapping/alignment/gap scenarios, and old Graph Overview reproduce
their failures on the base. They are outside this selection slice and remain
unfixed. Initial probes also exposed two pointer-down timing assumptions, now
updated and passing above. Do not describe these extended suites as fully passing.
Evidence includes initial/baseline logs and the two final isolated guards;
`baseline/` contains exact-base probes, not a second product implementation.

Candidate EXE: 10,000,896 bytes, SHA256
`09aa0d9170fd1a0817b52380b983198460744ce08e43b64cabdb2bef933c3d2e`.
Embedded bundle `index-T4R9GBQR.js`, SHA256
`d0917c2a1f4399d801640ad20b39fe81db83d514cd1468a94aef9d986065d6ff`.
Logs/fixture/screenshots: `test-results/canvas-artwork-marquee`, final native
receipt `native/receipt.json`. Sealed output: task folder
`canvas-marquee-artifact-8236aeb`. PR40 through PR46 candidates remain unchanged
by hash. No authored save-data change is introduced by this feature.

Not run: physical mouse/keyboard/IME, OS dialogs/cold-process file reopen, OS
scaling and broad graph/frame-time audits. Disabled native computer APIs were not
bypassed. The inherited test failures above are documented; no in-scope product
blocker remains.
