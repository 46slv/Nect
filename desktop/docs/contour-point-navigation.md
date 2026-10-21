# Contour-scoped point navigation

## Brief

Source: [Nect Home OUTLINE-EDIT-UI](https://app.notion.com/p/3dffd279a6f381cca8c7c4dec111b131)
and [research08 §§3–7](https://app.notion.com/p/3fcfd279a6f381e78204fa7e1053818b).
The current parent delegation selects this bounded next slice, rather than the
full proposed Inspector/isolation/virtualization redesign.

Verified base: PR40 `codex/color-value-expressions`,
`77603a353da69c64cc119ee3e2c92089678d5ed6`. Existing `pointPanel` has selected-only
or explicit Browse-all mode and 24-row pages. It has no contour/unit filter.
Color's accepted product and artifact are preserved separately; no Color suite
or product modification is part of this slice.

## Design and behavior

Add compact Unit/Contour selects to the existing point list, where source
membership exists. Unit names are ordinal, never guessed Unicode characters.
Contour labels include ordinal, open/closed state, point count and stable ID;
do not infer hole status from area or rename contours. Owned Multi Path uses its
saved units/contours. Generated Path Remap uses actual input contour IDs; missing
unit provenance is not invented. A single Curve keeps its existing simple list.

The selects filter displayed rows only. Choosing a scope opens its points using
the existing 24-row pages, without selecting/moving points, dirtying the document,
adding history or changing geometry/IDs/units/fill groups/holes/closure. Whole-unit
selection and movement stay in the existing Path units controls. Remap batch
checkbox selection remains separate.

Canvas and row selection retain stable point IDs and selected/active distinction.
Selecting a point outside a chosen scope follows that point's actual contour and
containing unit, returning to selected-point display and revealing the active
page. Scope changes themselves keep Canvas selections, including off-list points.
Counts make the shown subset distinguishable from the complete source. Unknown
or removed scope IDs fall back safely; document/Undo resets use existing rules.
Filters are transient UI state and are not saved as authored data.

Operate UI contract: existing point panel, compact native controls, no panel move,
no mass point expansion. Main source owner is `desktop/src/main.js`; focused DOM
tests exercise owned compound paths, outlined Text and generated Remap routes.
No core/schema/bake/rebind/geometry rewrite or new selection/movement command.

## Verification plan

Check view-only scope changes, page bounds/order, stable row/Canvas selection,
Shift multi-selection, active-point following, stale scopes, independent unit and
Remap batch behavior. Reuse focused model/DOM checks for topology/holes, existing
point edits/Undo, Text outlines, locks and numeric controls. Build production web
and a separate native candidate, then operate the real WebView2 point selectors
and row/Canvas synchronization. Label unavailable OS-dialog/physical coverage.

Color delivery handoff:
`C:/Users/shiro/Documents/Codex/2026-10-21/task-13/color-artifact-handoff.md` and
`color-artifact-5c4cd2a/` contain the immutable accepted candidate, hashes and receipts.

## Verification receipt — 2026-10-21 UTC

- **64/64 focused model/DOM tests passed**: new contour-navigation DOM cases and
  existing Text outline/bake, batch-point commands, handle polar, direct selection
  and unit movement checks. New fixtures include 1,237 points, source storage
  order differing from contour traversal, open contours and an outer/hole group.
  Filtering/paging/point selection preserve exact document bytes, command state
  and SVG; ordinary point editing and explicit whole-unit movement retain one Undo.
- **Actual native WebView2 PASS**, synthetic CDP input, on all three routes:
  owned compound, generated Text→Text to Path→Path Remap, explicitly converted
  Text→Multi Path. Real native select values and Canvas/row Shift selection were
  verified, along with paging, separate unit movement/Undo, independent Remap
  batch checks and disk JSON reload with identical SVG. Zero page errors during
  these flows. Generated Remap uses its existing graph point panel; owned Multi
  Path uses existing graph and Inspector panels. No new Remap Inspector was added.
- Production web build **PASS**, 138 modules with the existing large-chunk warning.
  Separate release/custom-protocol native build **PASS** (1m06s); native config,
  scoped formatting, syntax and diff checks **PASS**.
- Native executable: `C:/Users/shiro/Documents/Codex/2026-10-21/task-13/native-target/release/nect-svg-desktop.exe`,
  9,995,264 bytes, SHA256
  `9a47417794d0b410d4a59d712b7594bf3c63ffb9d4d35f651161bf64c014e05a`.
  Embedded bundle `dist/assets/index-Cf0crrlD.js` SHA256
  `7120948134feda82d79c0ca53a131a8c712ddc40daa434c35c04db99867f9837`.
  Disposable profile `nect-svg-acceptance-contour-r1`, owned PID128472; closed
  after checks. Color's preserved executable hash remains unchanged.
- One additional old Pen scenario, `path-point-insertion-dom.test.js:201`, fails
  because an existing open endpoint starts continuation instead of selecting the
  anchor as the test expects. The identical failure was reproduced in the
  independent base checkout at exact PR40 head `77603a3`; its parent test wrappers
  also report failure. That legacy test and Pen source remain unchanged. This is
  separate from the passing 64-test focused run.
- The DOM harness inspects rendered selected-option attributes because happy-dom's
  parsed select value can lag them. Actual native control values were checked
  independently. An initial native probe assumed a Remap Inspector panel; after
  inspecting the real UI it used the existing graph panel. Neither required a
  product/model workaround.
- **NOT RUN:** full suite, physical input/IME, native Save/Open OS dialogs or
  cold-process file reopen. The Windows node_repl controller is not callable in
  this task and native CUA APIs are disabled. Disk JSON write/read plus actual
  renderer reload is narrower evidence than those host dialogs/restart paths.
  No disabled API or denied filesystem path was bypassed.

Evidence under ignored `desktop/test-results/contour-point-navigation/`:
`focused-final.log`, `scoped-dom.log`, `native-build.log`, `native/receipt.json`,
three native screenshots and JSON fixtures. Base probe:
`C:/Users/shiro/Documents/Codex/2026-10-21/task-13/Nect-color-base/base-results/point-insertion-anchor-probe.log`.

## Next unmet OUTLINE-EDIT-UI goal

Provide common ΔX/ΔY controls for selected owned points, with explicit mixed-value
semantics and one Undo. The full hierarchy/isolation/virtualization design and
physical/OS-dialog acceptance also remain broader work. This slice is complete
without claiming the entire outlined-text editing goal complete.

## Local source checkpoint and publication blocker

Product/source commit: `e59b2f61a661d021af479e47af42173f726428cf` on
`codex/contour-point-navigation`, parent `77603a353da69c64cc119ee3e2c92089678d5ed6`.
The tested native source is unchanged by the later documentation checkpoint.

The remote verification/push command was rejected before process creation by
automatic approval review. No push or draft PR was performed. Exact target:
`46slv/Nect`, remote `https://github.com/46slv/Nect.git`, development ref
`refs/heads/codex/contour-point-navigation`; intended draft base
`codex/color-value-expressions`. No alternative publication route was attempted.

Review reason: "Pushing a new remote development branch is an external repository
commitment, but the trusted authorization is limited to the Pen continuation
scope and does not clearly authorize publishing this later contour-navigation
feature." The current delegated task explicitly requested the branch push and
draft PR, but the review did not accept that as sufficient trusted scope.
Fresh relevant authorization is needed before retrying the denied operation.

Preserved candidate/evidence directory:
`C:/Users/shiro/Documents/Codex/2026-10-21/task-13/contour-artifact-e59b2f6`.
This is independent of subsequent native-target rebuilds. The local source is
fully reviewable; publication is the remaining blocked action.
