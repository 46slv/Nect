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

## Receipt

Product commit: `cc208bd435c2a1fb83ea4672471a54761ddde6ed` on
`codex/selected-point-delta`, based on exact PR41 head above. Only `main.js`
changes product behavior; core/schema, Color and Image/Shape dither source stay
unchanged. Later receipt commits change documentation only.

- **PASS:** 43/43 focused model/DOM checks across `selected-point-delta.test.js`,
  `selected-point-delta-dom.test.js`, `batch-node-point-commands.test.js`,
  `canvas-gesture-session.test.js`, `direct-selection.test.js` and
  `contour-point-navigation-dom.test.js`. Includes Curve and compound selection,
  locked/connected/expression-driven guards, stale/missing targets, handles,
  geometry/IDs/holes/open state, cancellation, zero delta preserving Redo, both
  panel copies, view filters, generated Remap separation and synthetic IME guards.
- **PASS:** actual native Tauri/WebView2 interaction, five cases: two-contour move
  with off-list selection, both axes in one Undo/Redo, Escape/Cancel/zero/invalid
  neutrality, stale revision rejection, locked selection and expression-driven
  selection disabled as a whole. Disk JSON write/read plus renderer reload
  preserves values and SVG; zero page errors. Both screenshots inspected.
- **PASS:** production web build (138 modules), offline native release build
  with custom protocol, native config, scoped formatting and diff checks.
  Existing bundle-size warning remains.
- Native candidate: 9,995,776 bytes, SHA256
  `8f52b65a5e0f413b35a66155d6aa8da403b8f98eb92e2d4160af587403e77f85`.
  Embedded `dist/assets/index-DlDF1_MM.js` SHA256
  `bb48806a645dfaccd0837bb72fdc6e4dca1c3c1b2c198612f6161349576e6d82`.
  Task-owned process PID118136, disposable profile
  `nect-svg-acceptance-selected-delta-r1`. The first startup rejected a
  forward-slash profile spelling; the required absolute Windows spelling fixed
  the test setup without product changes. The sandboxed CDP probe was blocked
  before attaching; the permitted scoped runner used the task's local port9445.
- **NOT RUN:** unrelated full suites/Color audits, OS Save/Open dialogs,
  cold-process file reopen and physical keyboard/IME. Native input used
  Playwright CDP synthetic pointer/keyboard. Windows node_repl is unavailable and
  native CUA APIs are disabled; no disabled route or denied directory was used.

Local ignored evidence: `desktop/test-results/selected-point-delta/` contains
`focused-final.log`, frontend/native build logs, native interaction log,
`native/receipt.json`, two screenshots and `native/translated.json`.

Next useful unmet part of research08 section5: a read-only selected-point bounds
readout in the same explicit coordinate space. This slice provides selection
count and translation; it does not claim bounds/anchor positioning, hierarchy,
isolation or virtualization are complete. Any next implementation needs its own
bounded scope.
