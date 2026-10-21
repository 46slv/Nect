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

## Receipt

Product source: `f9a1b95b16f05e9137e1b64de6d83f34aa6c0507`.
Runtime delta: a pure `selected-point-bounds.js` query using existing
`layout.unionBounds`, point-panel rendering/focused refresh in `main.js`, and a
small six-value CSS grid. No core/schema/authored-state changes. Subsequent
checkpoint changes are receipt/native-test refinements only.

- **PASS:** 35/35 focused checks in `selected-point-bounds.test.js`,
  `selected-point-bounds-dom.test.js`, `selected-point-delta-dom.test.js` and
  `contour-point-navigation-dom.test.js`. The final bounds DOM recheck passed
  6/6 after singular-label polish. Covers 10,000 anchors, exact extrema, tiny
  and zero-size ranges, handles/unselected geometry, stable cross-contour IDs,
  filters, both panel copies, focused-editor refresh, edits and Undo/Redo,
  empty/stale/locked/driven/generated/invalid/overflow states and neutrality.
- **PASS:** actual native Tauri/WebView2, three cases: selection and point/delta
  edits with Undo/Redo, locked selection, expression-driven selection. Native
  output titles match exact anchor ranges; single and empty states, view-only
  browsing, disk JSON write/read plus renderer reload and equal SVG passed.
  Read-only summary clicking also preserved document and command state. Zero
  page errors; ready/unavailable screenshots inspected.
- A native fixture's short handle intercepted Shift-click on its already
  selected anchor. The test uses the existing stable-ID point-row Shift toggle
  for that deselection. No hit-test behavior was changed or claimed fixed.
  The DOM harness awaits the existing deferred numeric render and targets the
  current selection owner rather than the first graph node.
- **PASS:** production frontend (139 modules), native release/custom-protocol
  offline build, native config, scoped formatting and diff checks. Existing
  bundle-size warning remains.
- Native EXE: 9,996,288 bytes, SHA256
  `a5575c07e893fbc138cb6fcb0df838f86e713d452120c8c859e1040c696ce72b`.
  Embedded `index-Mv1VWM3_.js` SHA256
  `a82d6ba419a565578b45294db9525ea4468ae7acacc8375715a258e11b2424aa`.
  Owned PID130072, isolated `nect-svg-acceptance-selected-bounds-r1` profile.
- **NOT RUN:** unrelated full audits, physical input/IME, native OS Save/Open
  dialogs and cold-process file reopen. Native interaction used synthetic
  Playwright CDP input. Disabled native CUA and unavailable Windows node_repl
  routes were not bypassed.

Ignored evidence: `desktop/test-results/selected-point-bounds/` includes scoped
test/build/native logs, `native/receipt.json`, screenshots and `native/bounds.json`.
PR40–42 sealed candidates remain separate and unchanged.

Next useful unmet research08 section4 capability: explicit select-all/invert
within the current contour/unit list scope. Current selectors browse only;
whole-unit buttons and individual point selection are distinct existing actions.
No follow-on implementation is included here.
