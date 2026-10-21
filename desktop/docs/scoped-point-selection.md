# Scoped point selection

Base: verified PR44 `5e0b69adae9a6798df3e78fbee93a2c474b48929`, branch
`codex/color-operators-v1`. PR44's candidate/receipt were verified before a new
build in `C:/Users/shiro/Documents/Codex/2026-10-21/task-13/color-operators-artifact-c9ea676`,
SHA256 `d8d325786f0d20b6630261125271d495b480f021cdf036c5a2b4b17682fbcd22`.
Preserve all PR40–44 sealed candidates.

Source: adopted OUTLINE-EDIT-UI in [Nect Home](https://app.notion.com/p/3dffd279a6f381cca8c7c4dec111b131)
and research08 section4 in [research](https://app.notion.com/p/3fcfd279a6f381e78204fa7e1053818b).
Current point panels have view-only unit/contour selectors, 24-row paging,
individual stable-ID selection and distinct whole-unit actions; no scoped
select-all/invert equivalent exists.

Add explicit Select all / Invert buttons inside the existing point panel, with
the current path/unit/contour name and full target anchor count. Target membership
comes from all current source anchors in that scope, independent of paging and
selected-only display. Select all replaces selection with that set; invert
replaces it with that set's complement relative to current selection. Outside
points are dropped, not added or retained implicitly. An empty set disables the
actions. Active point/row follows a retained selected ID or the first new ID;
empty result clears focus. Existing Canvas selection remains the sole owner.

Source contract decision: locked/driven anchors are selectable already; including
them must not silently shrink the set. Keep them selected and retain existing
whole-selection edit rejection/disabled delta controls. Generated Remap uses its
actual current input contours; its independent checkbox batch state is unchanged.
No bake/rebind or held edit changes. Capture scope IDs, all target IDs, source
fingerprint, layer/session/revision in transient action tokens; stale targets or
ongoing edits reject before any selection change. Browsing may normalize an old
scope for display, but an old action token never falls back to another scope.

No authored geometry, schema, dirty/history, Color, dither, shortcut or panel
layout changes. Focus checks on empty/single/compound/hole/generated/outlined
sources, page-spanning and selected-only inversion, both panel copies, stable-ID
Canvas/active-row synchronization, locked/driven and stale rejection, and
document/file/history neutrality. Use scoped native renderer checks; label
unavailable physical input/IME, OS file dialogs and cold reopen separately.

## Verification receipt

Runtime source: `1a753f8f4b3f36cae565b9f5a9d4086412434220` (initial scope
implementation `aa6bcabf8a0419697773fb8b9d4060e19ca56303`, followed by the owned-path
render guard). Only `src/main.js` changes product behavior. No core/schema,
geometry, Color, dither or whole-unit movement changes.

- 28/28 focused DOM tests passed: scoped selection plus existing contour
  navigation, selected-point delta and selected-point bounds. The new suite has
  eight cases (nine Node test entries including the parent). It covers an empty
  owned path, ordinary Curve, compound holes/units, generated and explicitly
  outlined Text, all 40 anchors across 24-row pages, fixed-target inversion from
  zero selected rows, exact set replacement, both panel copies, stable active ID,
  stale scope/revision rejection, existing locked/driven edit rejection and
  document/file/history/Redo neutrality.
- Actual Windows Tauri/WebView2 interaction passed four cases: owned compound,
  generated Text Remap, owned outlined Text, and locked/driven anchors. Synthetic
  Playwright CDP clicks/selects exercised the real native renderer and Canvas
  markers. A 50-anchor unit includes its hole; a 10-anchor contour stays confined.
  Changing generated source topology uses only current IDs without Remap edits or
  rebind. Independent batch checkboxes remain separate. Zero page errors.
- Three disk JSON write/read plus renderer-load roundtrips preserve authored JSON
  and exported SVG; transient scope/selection resets. This is not a native
  Save/Open-dialog or cold-process file-reopen check.
- First native pass exposed duplicate Remap scope controls: permitting empty owned
  paths also rendered an empty panel for non-owned graph nodes. The final guard
  restricts owned panels to Curve/Multi Path; a regression assertion requires one
  Remap graph panel and none for its Text generator. The corrected native pass and
  all focused checks pass; initial diagnostics remain separate.
- Production frontend (140 modules), offline native release/custom-protocol
  build, native build configuration, scoped formatting and diff checks pass.
  Existing bundle-size warning remains. Native runtime PID 126556 was closed after
  checking its task-owned executable path.

Candidate: 9,998,336 bytes; EXE SHA256
`0f5b0658855c86007966342ea6161e345232ed4df33e2a45569136da38c56ac9`.
Embedded frontend `index-3rgz6P66.js`, SHA256
`8dc6b6425689699f184408a8a4067fada3de059adf1f15fa734ca3e689b3a48b`.
Evidence: `test-results/scoped-point-selection/focused-final.log`, build logs,
`native/receipt.json`, screenshots and JSON fixtures; sealed copy is maintained in
the task output `scoped-point-selection-artifact-1a753f8`.

Not run: unrelated full audits or Color/dither suites, separate browser rerun,
physical keyboard/IME, native OS Save/Open dialogs or cold-process file reopen.
Native computer APIs are disabled; the supported native WebView2 route above did
not bypass them. PR40 through PR44 sealed EXE hashes were rechecked unchanged.
