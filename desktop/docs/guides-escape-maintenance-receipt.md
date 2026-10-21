# Guides Escape maintenance receipt

Base: PR62 `codex/fireworks-ring-example` at verified remote
`a333a88a7ccebdc1b7158faf4a2fdfd69f49429e` in 46slv/Nect.
Product/build commit: `f7df56276f708d9f8954984a2e6c4ab7d3af7008`.
See [bounded diagnosis/brief](guides-escape-maintenance.md).

## Cause and repair

The original line76 null click was an obsolete panel assumption, already
reproduced with exact PR60 sources. Escape and outside clicks intentionally
dismiss the transient Guides panel. The inherited test also assumed Guides and
Layout Grid controls coexist. It now explicitly opens each existing surface
after dismissal/toolbar history commands, and tests scrolling on the surface
that owns the control. Guide/layout/grid/lock/preview/Undo assertions remain.
The locked-composition fixture now uses Session instead of mutating a snapshot.
One Undo assertion compares every model value/ID/ordered array with deep equality
rather than depending on JSON object member order. No skip or numeric tolerance
was introduced; preview/cancel JSON and SVG checks remain exact.

The connected-number-focus probe found a separate real guide-cancel defect.
PR55's numeric scrub repair remains correct, but guide drags could retain numeric
focus and hit both numeric-expression and later generic editing-control returns.
Escape did not release the Session gesture; late pointerup committed the guide.
This is the same early-return family as PR55, on a different gesture route.

Five added main.js lines give focused-control guide Escape ownership before both
returns and invoke the existing cancelInteraction/cancel-gesture. Body Escape,
typed numbers, ordinary guide movement and numeric scrubs keep their existing
paths. No core/schema/snapshot/history/geometry/selection/panel redesign, dither,
Repeater or Text implementation change.

## Observed verification

- 21 focused tests pass, zero failures/skips: repaired Guides DOM, pure Guides,
  nine-point Grid model/DOM, numeric pure/DOM and character numeric DOM. Guide
  pointercancel/body Escape/focused Escape require a real active preview, immediate
  gesture release, exact state after late move/up, unchanged file/revision/history
  flags and selected node IDs. Existing locks, hidden guides, snap, grid opacity,
  focused inputs, scrolling, preview, JSON/SVG and Undo/Redo execute to completion.
- Native Tauri/WebView2 on SHIRO-WS, one baseline/one patched case, CDP synthetic
  mouse/keyboard. Real hit target verified; a connected guide number input
  **naturally retained focus during pointer drag**, without refocusing after drag.
  Baseline sealed PR61 EXE:600→preview640, Escape retains guide-position;
  pointerup commits639.9999406368627, revision1 and Undo enabled. This is an
  expected defect diagnostic, not a baseline contract pass.
- Patched native: Escape immediately releases the gesture. After pointerup the
  guide remains600, revision0, Undo disabled; JSON/SVG/file/command/selected node
  identities exactly match the pre-drag state. Normal typed610 edit and one Undo
  restore the document. Page errors0 in both runs; both screenshots inspected.
- Frontend build, changed-file format/syntax, native build-config check and locked
  offline custom-protocol release build pass. Existing Vite chunk warning remains.
  Product inputs match the declared build source after documentation-only commits.
- Both dedicated dirty fixtures CLI-preserved before direct finish_close; owned
  PIDs108848/139780 exited, no force termination or user-app operation. Dedicated
  profile/Recovery paths were retained; no ordinary AppData access or deletion.
- Before/final preservation:20 prior EXE hashes plus PR61's56 and Fireworks Ring's26
  sealed files match. PR62 sample and comparison image remain unchanged.

The first narrow numeric-classification-only attempt did not fix the later
editing-control gate; that failed diagnostic is retained. The final handler sits
before both gates. No broader baseline/full/performance audit was rerun.

Physical/OS input, OS focus loss, native Save/Open/binding and clean OS cold reopen
remain unverified. CDP keyboard input and CLI fixture preservation are not OS
acceptance claims. No main merge, release or Library upload.

## Saved candidate

`guides-escape-artifact-f7df562/nect-svg-desktop.exe` in task-13.
Bytes:10003456. SHA256:
`dbe5aedd26dd4c4fd2b3aa3965780619206effb29644fd8cccc0b735cb0ee7ab`.

Separate output contains the built bundle, metadata/manifest, this brief/receipt,
focused/build logs, exact old-panel baseline log, native before/after observations,
screenshots, editable own fixtures, close and preservation checks. Runtime profiles
and Recovery are excluded. Publication metadata records final remote HEAD/PR.
